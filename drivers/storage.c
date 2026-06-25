#include "nova.h"

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH 0xE7

#define NOVAFS_LBA 32768u
#define NOVAFS_SECTORS 144
#define NOVAFS_BYTES (NOVAFS_SECTORS * 512)

static int storage_bus_ready = 0;

static void io_wait(void) {
    inb(0x80);
    inb(0x80);
    inb(0x80);
    inb(0x80);
}

static int ata_wait_ready(void) {
    for (int i = 0; i < 100000; i++) {
        u8 status = inb(ATA_STATUS);
        if (!(status & 0x80) && (status & 0x40)) return 1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        u8 status = inb(ATA_STATUS);
        if (status & 0x01) return 0;
        if (!(status & 0x80) && (status & 0x08)) return 1;
    }
    return 0;
}

static int ata_select_lba(u32 lba, u8 sectors) {
    if (!storage_bus_ready) return 0;
    if (!ata_wait_ready()) return 0;

    outb(ATA_DRIVE, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();
    outb(ATA_SECTOR_COUNT, sectors);
    outb(ATA_LBA_LOW, (u8)(lba & 0xFF));
    outb(ATA_LBA_MID, (u8)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    return 1;
}

static int ata_read_sector(u32 lba, u8* out) {
    if (!ata_select_lba(lba, 1)) return 0;
    outb(ATA_COMMAND, ATA_CMD_READ);
    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++) {
        u16 word = inw(ATA_DATA);
        out[i * 2] = (u8)(word & 0xFF);
        out[i * 2 + 1] = (u8)((word >> 8) & 0xFF);
    }
    return 1;
}

static int ata_write_sector(u32 lba, const u8* in) {
    if (!ata_select_lba(lba, 1)) return 0;
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++) {
        u16 word = (u16)in[i * 2] | ((u16)in[i * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready();
    return 1;
}

static int has_novafs_magic(const u8* sector) {
    return sector[0] == 'N' && sector[1] == 'O' && sector[2] == 'V' && sector[3] == 'A' &&
           sector[4] == 'F' && sector[5] == 'S' && sector[6] == '1';
}

void storage_init(void) {
    u8 status = inb(ATA_STATUS);
    storage_bus_ready = status != 0xFF && status != 0x00;
}

int storage_ready(void) {
    return storage_bus_ready;
}

const char* storage_status_text(void) {
    if (storage_bus_ready) return "Storage bus visible, NovaFS commands available";
    return "Storage safe mode, no disk detected";
}

int storage_format_novafs(void) {
    if (!storage_bus_ready) return 0;

    u8 sector[512];
    for (int i = 0; i < 512; i++) sector[i] = 0;
    sector[0] = 'N';
    sector[1] = 'O';
    sector[2] = 'V';
    sector[3] = 'A';
    sector[4] = 'F';
    sector[5] = 'S';
    sector[6] = '1';
    sector[8] = (u8)NOVAFS_SECTORS;

    if (!ata_write_sector(NOVAFS_LBA, sector)) return 0;
    return 1;
}

int storage_read_novafs(u8* buffer, int bytes) {
    if (!storage_bus_ready || !buffer || bytes > NOVAFS_BYTES) return 0;

    u8 first[512];
    if (!ata_read_sector(NOVAFS_LBA, first)) return 0;
    if (!has_novafs_magic(first)) return 0;

    int pos = 0;
    for (int s = 0; s < NOVAFS_SECTORS && pos < bytes; s++) {
        u8 sector[512];
        if (s == 0) {
            for (int i = 0; i < 512; i++) sector[i] = first[i];
        } else if (!ata_read_sector(NOVAFS_LBA + (u32)s, sector)) {
            return 0;
        }
        for (int i = 0; i < 512 && pos < bytes; i++) buffer[pos++] = sector[i];
    }
    return 1;
}

int storage_write_novafs(const u8* buffer, int bytes) {
    if (!storage_bus_ready || !buffer || bytes > NOVAFS_BYTES) return 0;
    if (!has_novafs_magic(buffer)) return 0;

    u8 first[512];
    if (!ata_read_sector(NOVAFS_LBA, first)) return 0;
    if (!has_novafs_magic(first)) return 0;

    int pos = 0;
    for (int s = 0; s < NOVAFS_SECTORS; s++) {
        u8 sector[512];
        for (int i = 0; i < 512; i++) {
            sector[i] = pos < bytes ? buffer[pos] : 0;
            pos++;
        }
        if (!ata_write_sector(NOVAFS_LBA + (u32)s, sector)) return 0;
    }
    return 1;
}
