#include <stdint.h>
#include <stddef.h>

#include "io.h"
#include "ide.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_REG_DATA        (ATA_PRIMARY_IO + 0)
#define ATA_REG_SECCOUNT0   (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA0        (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA1        (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA2        (ATA_PRIMARY_IO + 5)
#define ATA_REG_HDDEVSEL    (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND     (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS      (ATA_PRIMARY_IO + 7)
#define ATA_REG_ALTSTATUS   (ATA_PRIMARY_CTRL + 0)

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_ERR 0x01
#define ATA_SR_DF  0x20
#define ATA_SR_DRQ 0x08
#define ATA_SR_BSY 0x80

#define ATA_WAIT_TIMEOUT 1000000

static void ata_io_delay(void) {
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
}

static int ata_wait_not_busy(void) {
    unsigned char status;
    uint32_t timeout = ATA_WAIT_TIMEOUT;

    do {
        status = inb(ATA_REG_STATUS);
        if (timeout-- == 0) {
            return -1;
        }
    } while (status & ATA_SR_BSY);

    if (status & (ATA_SR_ERR | ATA_SR_DF)) {
        return -1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    unsigned char status;
    uint32_t timeout = ATA_WAIT_TIMEOUT;

    for (;;) {
        status = inb(ATA_REG_STATUS);

        if (timeout-- == 0) {
            return -1;
        }

        if (status & ATA_SR_ERR) {
            return -1;
        }
        if (status & ATA_SR_DF) {
            return -1;
        }
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
}

static int ata_select_lba28(uint32_t lba) {
    if (lba > 0x0FFFFFFF) {
        return -1;
    }

    if (ata_wait_not_busy() != 0) {
        return -1;
    }


    outb(ATA_REG_HDDEVSEL, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_io_delay();

    outb(ATA_REG_SECCOUNT0, 1);
    outb(ATA_REG_LBA0, (unsigned char)(lba & 0xFF));
    outb(ATA_REG_LBA1, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (unsigned char)((lba >> 16) & 0xFF));
    return 0;
}

int ide_read_sector(uint32_t lba, void *buffer) {
    uint16_t *dst = (uint16_t *)buffer;
    int i;

    if (buffer == NULL) {
        return -1;
    }

    if (ata_select_lba28(lba) != 0) {
        return -1;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (i = 0; i < 256; i++) {
        dst[i] = inw(ATA_REG_DATA);
    }

    return 0;
}

int ide_write_sector(uint32_t lba, const void *buffer) {
    const uint16_t *src = (const uint16_t *)buffer;
    int i;

    if (buffer == NULL) {
        return -1;
    }

    if (ata_select_lba28(lba) != 0) {
        return -1;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (i = 0; i < 256; i++) {
        outw(ATA_REG_DATA, src[i]);
    }

    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_not_busy() != 0) {
        return -1;
    }

    return 0;
}
