#include <stdint.h>
#include "ide.h"

/*
 * ATA PIO ports
 */
#define ATA_DATA       0x1F0
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA0       0x1F3
#define ATA_LBA1       0x1F4
#define ATA_LBA2       0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_CMD        0x1F7

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30

#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DRQ      0x08

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ata_wait_busy() {
    while (inb(ATA_STATUS) & ATA_SR_BSY) {}
}

static void ata_wait_drq() {
    while (!(inb(ATA_STATUS) & ATA_SR_DRQ)) {}
}

/*
 * Read 1 sector (512 bytes)
 */
void ide_read_sector(uint32_t lba, uint8_t* buf) {
    ata_wait_busy();

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_CMD, ATA_CMD_READ);

    ata_wait_busy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++)
        ((uint16_t*)buf)[i] = inw(ATA_DATA);
}

/*
 * Write 1 sector (512 bytes)
 */
void ide_write_sector(uint32_t lba, const uint8_t* buf) {
    ata_wait_busy();

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_CMD, ATA_CMD_WRITE);

    ata_wait_busy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, ((const uint16_t*)buf)[i]);

    // flush
    ata_wait_busy();
}
