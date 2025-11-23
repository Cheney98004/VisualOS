#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

# =============================
# FAT16 CONSTANTS
# =============================
BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 1
NUM_FATS = 2
FAT_SIZE_SECTORS = 9
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880  # 1.44MB floppy

ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32 + (BYTES_PER_SECTOR - 1)) // BYTES_PER_SECTOR
DATA_START_SECTOR = RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS + ROOT_DIR_SECTORS


def write_word(buf: bytearray, offset: int, value: int):
    struct.pack_into("<H", buf, offset, value)


def write_dword(buf: bytearray, offset: int, value: int):
    struct.pack_into("<I", buf, offset, value)


def alloc_contiguous(fat: bytearray, start_cluster: int, cluster_count: int):
    """Allocate a contiguous region of clusters in FAT."""
    for i in range(cluster_count):
        curr = start_cluster + i
        next_cluster = 0xFFFF if i == cluster_count - 1 else (curr + 1)
        write_word(fat, curr * 2, next_cluster)


# ===============================================================
# CREATE IMAGE
# ===============================================================
def create_image(boot_path: Path, kernel_path: Path, help_path: Path, output_img: Path):

    # --------------------------
    # Load binaries
    # --------------------------
    boot_bin = boot_path.read_bytes()
    if len(boot_bin) != 512:
        raise ValueError("Bootloader must be exactly 512 bytes.")

    kernel_bin = kernel_path.read_bytes()
    help_bin = help_path.read_bytes()

    kernel_size = len(kernel_bin)
    help_size = len(help_bin)

    # Allocate full disk image buffer
    img = bytearray(TOTAL_SECTORS * BYTES_PER_SECTOR)
    img[0:512] = boot_bin

    # --------------------------
    # Build FAT table
    # --------------------------
    fat_table = bytearray(FAT_SIZE_SECTORS * BYTES_PER_SECTOR)

    write_word(fat_table, 0, 0xFFF0)  # media descriptor
    write_word(fat_table, 2, 0xFFFF)  # reserved

    next_free_cluster = 2  # cluster index starts at 2

    # ------------------------------
    # Allocate kernel clusters
    # ------------------------------
    kernel_clusters = (kernel_size + 511) // 512
    start_kernel_cluster = next_free_cluster
    alloc_contiguous(fat_table, start_kernel_cluster, kernel_clusters)
    next_free_cluster += kernel_clusters

    # ------------------------------
    # Allocate /ETC directory cluster
    # ------------------------------
    start_etc_cluster = next_free_cluster
    alloc_contiguous(fat_table, start_etc_cluster, 1)
    next_free_cluster += 1

    # ------------------------------
    # Allocate help.txt clusters
    # ------------------------------
    help_clusters = (help_size + 511) // 512
    start_help_cluster = next_free_cluster
    alloc_contiguous(fat_table, start_help_cluster, help_clusters)
    next_free_cluster += help_clusters

    # ------------------------------
    # Write FAT copies
    # ------------------------------
    for fi in range(NUM_FATS):
        off = (RESERVED_SECTORS + fi * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
        img[off:off + len(fat_table)] = fat_table

    # ===============================================================
    # BUILD ROOT DIRECTORY
    # ===============================================================
    root_dir = bytearray(ROOT_DIR_SECTORS * BYTES_PER_SECTOR)

    # Entry 0: KERNEL.BIN
    root_dir[0:11] = b"KERNEL  BIN"
    root_dir[11] = 0x20
    write_word(root_dir, 26, start_kernel_cluster)
    write_dword(root_dir, 28, kernel_size)

    # Root entry: ETC directory
    root_dir[32:40] = b"ETC     "   # name (8 bytes)
    root_dir[40:43] = b"   "        # ext  (3 bytes)
    root_dir[43] = 0x10             # directory attribute
    write_word(root_dir, 32+26, start_etc_cluster)
    write_dword(root_dir, 32+28, 0)

    # write root directory to image
    root_start = (RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
    img[root_start:root_start + len(root_dir)] = root_dir

    # --------------------------
    # Build ETC directory (1 cluster = 512 bytes)
    # --------------------------
    etc_dir = bytearray(512)

    # "."
    etc_dir[0:8] = b".       "
    etc_dir[8:11] = b"   "
    etc_dir[11] = 0x10
    write_word(etc_dir, 26, start_etc_cluster)

    # ".."
    etc_dir[32:40] = b"..      "
    etc_dir[40:43] = b"   "
    etc_dir[43] = 0x10
    write_word(etc_dir, 32+26, 0)

    # HELP.TXT
    etc_dir[64:72] = b"HELP    "
    etc_dir[72:75] = b"TXT"
    etc_dir[75] = 0x20
    write_word(etc_dir, 64+26, start_help_cluster)
    write_dword(etc_dir, 64+28, help_size)

    # Write ETC directory cluster
    etc_offset = DATA_START_SECTOR * 512 + (start_etc_cluster - 2) * 512
    img[etc_offset:etc_offset + 512] = etc_dir

    # ===============================================================
    # Write Kernel file data
    # ===============================================================
    kernel_offset = DATA_START_SECTOR * 512 + (start_kernel_cluster - 2) * 512
    img[kernel_offset:kernel_offset + kernel_size] = kernel_bin

    # ===============================================================
    # Write help.txt data
    # ===============================================================
    help_offset = DATA_START_SECTOR * 512 + (start_help_cluster - 2) * 512
    img[help_offset:help_offset + help_size] = help_bin

    # ===============================================================
    # Save disk image
    # ===============================================================
    output_img.write_bytes(img)

    print(f"Created FAT16 image: {output_img}")
    print(f" - KERNEL.BIN cluster {start_kernel_cluster}, size={kernel_size}")
    print(f" - ETC directory cluster {start_etc_cluster}")
    print(f" - /ETC/help.txt cluster {start_help_cluster}, size={help_size}")


# ===============================================================
# MAIN
# ===============================================================
def main():
    parser = argparse.ArgumentParser(description="FAT16 Image Builder (kernel + /ETC/help.txt)")
    parser.add_argument("boot_bin", type=Path)
    parser.add_argument("kernel_bin", type=Path)
    parser.add_argument("help_txt", type=Path)
    parser.add_argument("output_img", type=Path)
    args = parser.parse_args()

    create_image(args.boot_bin, args.kernel_bin, args.help_txt, args.output_img)


if __name__ == "__main__":
    main()
