#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path


BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 1
NUM_FATS = 2
FAT_SIZE_SECTORS = 9
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880  # 1.44MB floppy

ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32 + (BYTES_PER_SECTOR - 1)) // BYTES_PER_SECTOR
DATA_START_SECTOR = RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS + ROOT_DIR_SECTORS


def write_word(buf: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", buf, offset, value)


def write_dword(buf: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", buf, offset, value)


def create_image(boot_path: Path, kernel_path: Path, output_img: Path) -> None:
    boot_bin = boot_path.read_bytes()
    if len(boot_bin) != BYTES_PER_SECTOR:
        raise ValueError(f"Boot sector must be exactly {BYTES_PER_SECTOR} bytes (got {len(boot_bin)})")

    kernel_bin = kernel_path.read_bytes()
    kernel_size = len(kernel_bin)

    total_size = TOTAL_SECTORS * BYTES_PER_SECTOR
    img = bytearray(total_size)

    # copy boot sector (already contains BPB)
    img[0:BYTES_PER_SECTOR] = boot_bin

    # build FAT
    fat_entries = (total_size // BYTES_PER_SECTOR)  # generous upper bound
    fat_table = bytearray(FAT_SIZE_SECTORS * BYTES_PER_SECTOR)

    # reserved entries
    write_word(fat_table, 0, 0xFFF0)  # media descriptor F0 + reserved bits
    write_word(fat_table, 2, 0xFFFF)

    # allocate clusters for kernel (contiguous)
    data_sectors = TOTAL_SECTORS - (RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS + ROOT_DIR_SECTORS)
    clusters_available = data_sectors // SECTORS_PER_CLUSTER
    sectors_needed = (kernel_size + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
    clusters_needed = (sectors_needed + SECTORS_PER_CLUSTER - 1) // SECTORS_PER_CLUSTER

    if clusters_needed > clusters_available:
        raise ValueError("Kernel does not fit in FAT16 image")

    start_cluster = 2
    for i in range(clusters_needed):
        current = start_cluster + i
        next_cluster = 0xFFFF if i == clusters_needed - 1 else (current + 1)
        write_word(fat_table, current * 2, next_cluster)

    # copy FAT to both tables
    for fat_index in range(NUM_FATS):
        start = (RESERVED_SECTORS + fat_index * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
        img[start:start + len(fat_table)] = fat_table

    # root directory with one file KERNEL.BIN
    root_dir = bytearray(ROOT_DIR_SECTORS * BYTES_PER_SECTOR)
    name = b"KERNEL  BIN"  # 8.3 padded
    root_dir[0:11] = name
    root_dir[11] = 0x20  # attribute: archive
    write_word(root_dir, 26, start_cluster)
    write_dword(root_dir, 28, kernel_size)

    root_start = (RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
    img[root_start:root_start + len(root_dir)] = root_dir

    # write kernel data (assume contiguous)
    data_start = DATA_START_SECTOR * BYTES_PER_SECTOR
    kernel_offset = data_start + (start_cluster - 2) * SECTORS_PER_CLUSTER * BYTES_PER_SECTOR
    img[kernel_offset:kernel_offset + kernel_size] = kernel_bin

    output_img.write_bytes(img)
    print(f"Created FAT16 image {output_img} (kernel {kernel_size} bytes, start cluster {start_cluster})")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create FAT16 image with boot sector and kernel")
    parser.add_argument("boot_bin", type=Path, help="512-byte boot sector binary")
    parser.add_argument("kernel_bin", type=Path, help="flat kernel binary")
    parser.add_argument("output_img", type=Path, help="output FAT16 image path")
    args = parser.parse_args()

    create_image(args.boot_bin, args.kernel_bin, args.output_img)


if __name__ == "__main__":
    main()
