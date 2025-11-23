#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path
import math

# -------------------------------------------------------------------
# FAT16 Constants
# -------------------------------------------------------------------
SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED = 1
FATS = 2
FAT_SIZE = 9
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880

ROOT_SECTORS = (ROOT_ENTRIES * 32 + SECTOR - 1) // SECTOR
DATA_START = RESERVED + FATS * FAT_SIZE + ROOT_SECTORS

# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------
def w16(buf, off, v): struct.pack_into("<H", buf, off, v)
def w32(buf, off, v): struct.pack_into("<I", buf, off, v)

def to_83(name: str):
    name = name.upper()
    if "." in name:
        b, e = name.split(".", 1)
    else:
        b, e = name, ""
    b = b[:8].ljust(8)
    e = e[:3].ljust(3)
    return (b + e).encode("ascii")

def alloc_chain(fat, start, count):
    for i in range(count):
        cur = start + i
        nxt = 0xFFFF if i == count - 1 else (cur + 1)
        w16(fat, cur * 2, nxt)

def write_cluster(img, cl, data):
    start = DATA_START * SECTOR + (cl - 2) * SECTOR
    img[start:start+len(data)] = data

# -------------------------------------------------------------------
# Directory entry
# -------------------------------------------------------------------
def mk_entry(name83, attr, cluster, size):
    e = bytearray(32)
    e[0:11] = name83
    e[11] = attr
    w16(e, 26, cluster)
    w32(e, 28, size)
    return e

# -------------------------------------------------------------------
# Main Image Builder
# -------------------------------------------------------------------
def create_image(bootbin, kernelbin, helpfile, snakefile, testfile, outimg):

    # --- read inputs ---
    boot = bootbin.read_bytes()
    if len(boot) != 512:
        raise ValueError("Bootloader must be exactly 512 bytes")

    kernel = kernelbin.read_bytes()
    helpdata = helpfile.read_bytes()
    snake = snakefile.read_bytes()
    test = testfile.read_bytes()

    img = bytearray(TOTAL_SECTORS * SECTOR)

    # write boot sector
    img[0:512] = boot

    # --- FAT buffer ---
    fat = bytearray(FAT_SIZE * SECTOR)
    w16(fat, 0, 0xFFF0)
    w16(fat, 2, 0xFFFF)

    nextcl = 2

    # --- allocate clusters ---
    def alloc(filedata):
        nonlocal nextcl
        start = nextcl
        cnt = math.ceil(len(filedata) / SECTOR)
        alloc_chain(fat, start, cnt)
        nextcl += cnt
        return start, cnt

    kcl, _ = alloc(kernel)
    hcl, _ = alloc(helpdata)
    snake_cl, _ = alloc(snake)
    test_cl, _ = alloc(test)

    # --- Write FATs ---
    for i in range(FATS):
        off = (RESERVED + i * FAT_SIZE) * SECTOR
        img[off:off+len(fat)] = fat

    # --- Root Directory (4 files) ---
    root = bytearray(ROOT_SECTORS * SECTOR)
    entries = [
        mk_entry(to_83("KERNEL.BIN"), 0x20, kcl, len(kernel)),
        mk_entry(to_83("HELP.TXT"),   0x20, hcl, len(helpdata)),
        mk_entry(to_83(snakefile.name), 0x20, snake_cl, len(snake)),
        mk_entry(to_83(testfile.name), 0x20, test_cl, len(test)),
    ]

    for i, e in enumerate(entries):
        root[i*32:(i+1)*32] = e

    root_off = (RESERVED + FATS * FAT_SIZE) * SECTOR
    img[root_off:root_off+len(root)] = root

    # --- Data clusters ---
    write_cluster(img, kcl, kernel)
    write_cluster(img, hcl, helpdata)
    write_cluster(img, snake_cl, snake)
    write_cluster(img, test_cl, test)

    # --- Output ---
    outimg.write_bytes(img)

    print("✔ FAT16 image created:", outimg)
    print("  kernel @", kcl)
    print("  help   @", hcl)
    print("  snake  @", snake_cl)
    print("  test   @", test_cl)

# -------------------------------------------------------------------
# CLI
# -------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="VisualOS FAT16 Builder")
    parser.add_argument("boot_bin", type=Path)
    parser.add_argument("kernel_bin", type=Path)
    parser.add_argument("help_txt", type=Path)
    parser.add_argument("snake_elf", type=Path)
    parser.add_argument("test_elf", type=Path)
    parser.add_argument("output_img", type=Path)
    args = parser.parse_args()

    create_image(
        args.boot_bin,
        args.kernel_bin,
        args.help_txt,
        args.snake_elf,
        args.test_elf,
        args.output_img
    )

if __name__ == "__main__":
    main()
