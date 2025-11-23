#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path
import math

# ===============================================================
# FAT16 PARAMETERS
# ===============================================================
BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 1
NUM_FATS = 2
FAT_SIZE_SECTORS = 9
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880   # 1.44MB floppy

ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
DATA_START_SECTOR = RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS + ROOT_DIR_SECTORS

# ===============================================================
# Helpers
# ===============================================================

def write_word(buf, offset, value):
    struct.pack_into("<H", buf, offset, value)

def write_dword(buf, offset, value):
    struct.pack_into("<I", buf, offset, value)

def to_83(name: str):
    """
    Convert "hello.elf" → "HELLO   ELF"
    """
    name = name.upper()
    if "." in name:
        base, ext = name.split(".", 1)
    else:
        base, ext = name, ""

    base = (base[:8]).ljust(8)
    ext  = (ext[:3]).ljust(3)

    return (base + ext).encode("ascii")

def alloc_clusters(fat, start_cluster, clusters):
    """Allocate a contiguous chain in FAT16."""
    for i in range(clusters):
        curr = start_cluster + i
        next = 0xFFFF if i == clusters - 1 else (curr + 1)
        write_word(fat, curr * 2, next)

def write_file_data(img, start_cluster, data):
    """Write file data into the data region."""
    offset = DATA_START_SECTOR * BYTES_PER_SECTOR + (start_cluster - 2) * BYTES_PER_SECTOR
    img[offset:offset+len(data)] = data

# ===============================================================
# Directory entry builder
# ===============================================================

def make_dir_entry(name83: bytes, attr: int, cluster: int, size: int):
    e = bytearray(32)
    e[0:11] = name83
    e[11] = attr
    write_word(e, 26, cluster)
    write_dword(e, 28, size)
    return e

# ===============================================================
# Main Image Builder
# ===============================================================

def create_image(boot_bin: Path,
                 kernel_bin: Path,
                 help_txt: Path,
                 app_elf: Path,
                 output_img: Path):

    boot_bytes = boot_bin.read_bytes()
    if len(boot_bytes) != 512:
        raise ValueError("Bootloader must be exactly 512 bytes.")

    kernel_bytes = kernel_bin.read_bytes()
    help_bytes   = help_txt.read_bytes()
    app_bytes    = app_elf.read_bytes()

    # Allocate entire floppy
    img = bytearray(TOTAL_SECTORS * BYTES_PER_SECTOR)
    img[0:512] = boot_bytes

    # FAT table
    fat = bytearray(FAT_SIZE_SECTORS * BYTES_PER_SECTOR)
    write_word(fat, 0, 0xFFF0)  # media descriptor
    write_word(fat, 2, 0xFFFF)  # reserved

    next_free_cluster = 2

    # =========================================================
    # Allocate kernel
    # =========================================================
    kernel_clusters = math.ceil(len(kernel_bytes) / 512)
    kernel_start_cluster = next_free_cluster
    alloc_clusters(fat, kernel_start_cluster, kernel_clusters)
    next_free_cluster += kernel_clusters

    # =========================================================
    # Allocate /ETC directory
    # =========================================================
    etc_cluster = next_free_cluster
    alloc_clusters(fat, etc_cluster, 1)
    next_free_cluster += 1

    # =========================================================
    # Allocate help.txt under /ETC
    # =========================================================
    help_clusters = math.ceil(len(help_bytes) / 512)
    help_start_cluster = next_free_cluster
    alloc_clusters(fat, help_start_cluster, help_clusters)
    next_free_cluster += help_clusters

    # =========================================================
    # Allocate /APPS directory
    # =========================================================
    apps_cluster = next_free_cluster
    alloc_clusters(fat, apps_cluster, 1)
    next_free_cluster += 1

    # =========================================================
    # Allocate hello.elf under /APPS
    # =========================================================
    app_clusters = math.ceil(len(app_bytes) / 512)
    app_start_cluster = next_free_cluster
    alloc_clusters(fat, app_start_cluster, app_clusters)
    next_free_cluster += app_clusters

    # =========================================================
    # Write FAT tables
    # =========================================================
    for f in range(NUM_FATS):
        offset = (RESERVED_SECTORS + f * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
        img[offset:offset+len(fat)] = fat

    # =========================================================
    # Build root directory
    # =========================================================
    root = bytearray(ROOT_DIR_SECTORS * BYTES_PER_SECTOR)

    # File 1: KERNEL.BIN
    root[0:32] = make_dir_entry(
        to_83("KERNEL.BIN"),
        0x20,
        kernel_start_cluster,
        len(kernel_bytes)
    )

    # Directory 2: ETC
    root[32:64] = make_dir_entry(
        to_83("ETC"),
        0x10,
        etc_cluster,
        0
    )

    # Directory 3: APPS
    root[64:96] = make_dir_entry(
        to_83("APPS"),
        0x10,
        apps_cluster,
        0
    )

    # Write root
    root_offset = (RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
    img[root_offset:root_offset+len(root)] = root

    # =========================================================
    # Build ETC directory (1 cluster)
    # =========================================================
    etc_dir = bytearray(512)

    # "."
    etc_dir[0:32] = make_dir_entry(to_83("."), 0x10, etc_cluster, 0)

    # ".."
    etc_dir[32:64] = make_dir_entry(to_83(".."), 0x10, 0, 0)

    # HELP.TXT
    etc_dir[64:96] = make_dir_entry(
        to_83("HELP.TXT"),
        0x20,
        help_start_cluster,
        len(help_bytes)
    )

    etc_offset = DATA_START_SECTOR * 512 + (etc_cluster - 2) * 512
    img[etc_offset:etc_offset+512] = etc_dir

    # =========================================================
    # Build APPS directory (1 cluster)
    # =========================================================
    apps_dir = bytearray(512)

    apps_dir[0:32] = make_dir_entry(to_83("."), 0x10, apps_cluster, 0)
    apps_dir[32:64] = make_dir_entry(to_83(".."), 0x10, 0, 0)

    apps_dir[64:96] = make_dir_entry(
        to_83(app_elf.name),
        0x20,
        app_start_cluster,
        len(app_bytes)
    )

    apps_offset = DATA_START_SECTOR * 512 + (apps_cluster - 2) * 512
    img[apps_offset:apps_offset+512] = apps_dir

    # =========================================================
    # Write file data
    # =========================================================
    write_file_data(img, kernel_start_cluster, kernel_bytes)
    write_file_data(img, help_start_cluster, help_bytes)
    write_file_data(img, app_start_cluster, app_bytes)

    # =========================================================
    # Save image
    # =========================================================
    output_img.write_bytes(img)

    print("Created FAT16 image:", output_img)
    print(f" - KERNEL.BIN at cluster {kernel_start_cluster}")
    print(f" - /ETC/help.txt at cluster {help_start_cluster}")
    print(f" - /APPS/{app_elf.name} at cluster {app_start_cluster}")
    print("Done!")

# ===============================================================
# MAIN
# ===============================================================

def main():
    parser = argparse.ArgumentParser(description="FAT16 Image Builder")
    parser.add_argument("boot_bin", type=Path)
    parser.add_argument("kernel_bin", type=Path)
    parser.add_argument("help_txt", type=Path)
    parser.add_argument("app_elf", type=Path)
    parser.add_argument("output_img", type=Path)
    args = parser.parse_args()

    create_image(args.boot_bin,
                 args.kernel_bin,
                 args.help_txt,
                 args.app_elf,
                 args.output_img)

if __name__ == "__main__":
    main()
