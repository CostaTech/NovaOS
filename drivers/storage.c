#include "nova.h"

// ============================================================
//  NovaOS Storage Driver — ATA PIO con timeout robusto
// ============================================================

#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE        0x1F6
#define ATA_STATUS       0x1F7
#define ATA_COMMAND      0x1F7
#define ATA_CONTROL      0x3F6   // alternate status / device control

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30
#define ATA_CMD_FLUSH    0xE7
#define ATA_CMD_IDENTIFY 0xEC

// Status bits
#define ATA_SR_BSY  0x80   // busy
#define ATA_SR_DRDY 0x40   // drive ready
#define ATA_SR_DRQ  0x08   // data request
#define ATA_SR_ERR  0x01   // error

// Timeout: numero di iterazioni prima di considerare il disco bloccato
// ~400ns per iterazione su hardware reale → ~400ms totali
#define ATA_TIMEOUT_READY  1000000
#define ATA_TIMEOUT_DRQ    1000000

#define NOVAFS_LBA     32768u
#define NOVAFS_SECTORS 144
#define NOVAFS_BYTES   (NOVAFS_SECTORS * 512)

// ── Codici errore ──────────────────────────────────────────
#define ATA_OK           0
#define ATA_ERR_NO_DISK  1   // disco non presente
#define ATA_ERR_TIMEOUT  2   // disco non risponde
#define ATA_ERR_FAULT    3   // errore hardware (bit ERR settato)
#define ATA_ERR_MAGIC    4   // magic NovaFS non trovato
#define ATA_ERR_PARAM    5   // parametri non validi

static int storage_bus_ready = 0;
static int last_error = ATA_OK;

// ── I/O helpers ────────────────────────────────────────────
static void io_wait(void) {
    // Legge il registro alternate status (porta 0x3F6) 4 volte
    // per garantire ~400ns di delay, come da spec ATA
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
}

// ── Reset software del controller ATA ─────────────────────
static void ata_software_reset(void) {
    outb(ATA_CONTROL, 0x04);  // SRST bit
    io_wait();
    io_wait();
    outb(ATA_CONTROL, 0x00);  // clear SRST
    io_wait();
    io_wait();
}

// ── Wait con timeout robusto ───────────────────────────────
// Aspetta che BSY=0 e DRDY=1
// Ritorna ATA_OK, ATA_ERR_TIMEOUT, o ATA_ERR_FAULT
static int ata_wait_ready(void) {
    for (u32 i = 0; i < ATA_TIMEOUT_READY; i++) {
        u8 status = inb(ATA_STATUS);
        if (status == 0xFF) { last_error = ATA_ERR_NO_DISK;  return ATA_ERR_NO_DISK; }
        if (status & ATA_SR_ERR) { last_error = ATA_ERR_FAULT; return ATA_ERR_FAULT; }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) return ATA_OK;
    }
    // Timeout: prova reset e ritenta una volta
    ata_software_reset();
    for (u32 i = 0; i < ATA_TIMEOUT_READY; i++) {
        u8 status = inb(ATA_STATUS);
        if (status == 0xFF) { last_error = ATA_ERR_NO_DISK;  return ATA_ERR_NO_DISK; }
        if (status & ATA_SR_ERR) { last_error = ATA_ERR_FAULT; return ATA_ERR_FAULT; }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) return ATA_OK;
    }
    last_error = ATA_ERR_TIMEOUT;
    return ATA_ERR_TIMEOUT;
}

// Aspetta DRQ=1 (disco pronto a trasferire dati)
static int ata_wait_drq(void) {
    for (u32 i = 0; i < ATA_TIMEOUT_DRQ; i++) {
        u8 status = inb(ATA_STATUS);
        if (status == 0xFF) { last_error = ATA_ERR_NO_DISK;  return ATA_ERR_NO_DISK; }
        if (status & ATA_SR_ERR) { last_error = ATA_ERR_FAULT; return ATA_ERR_FAULT; }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))  return ATA_OK;
    }
    last_error = ATA_ERR_TIMEOUT;
    return ATA_ERR_TIMEOUT;
}

// ── Seleziona LBA e numero settori ─────────────────────────
static int ata_select_lba(u32 lba, u8 sectors) {
    if (!storage_bus_ready) { last_error = ATA_ERR_NO_DISK; return ATA_ERR_NO_DISK; }
    int r = ata_wait_ready();
    if (r != ATA_OK) return r;

    outb(ATA_DRIVE, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();
    outb(ATA_SECTOR_COUNT, sectors);
    outb(ATA_LBA_LOW,  (u8)(lba & 0xFF));
    outb(ATA_LBA_MID,  (u8)((lba >> 8)  & 0xFF));
    outb(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    return ATA_OK;
}

// ── Lettura settore ────────────────────────────────────────
static int ata_read_sector(u32 lba, u8* out) {
    int r = ata_select_lba(lba, 1);
    if (r != ATA_OK) return r;

    outb(ATA_COMMAND, ATA_CMD_READ);
    io_wait();

    r = ata_wait_drq();
    if (r != ATA_OK) return r;

    for (int i = 0; i < 256; i++) {
        u16 word = inw(ATA_DATA);
        out[i * 2]     = (u8)(word & 0xFF);
        out[i * 2 + 1] = (u8)((word >> 8) & 0xFF);
    }
    return ATA_OK;
}

// ── Scrittura settore ──────────────────────────────────────
static int ata_write_sector(u32 lba, const u8* in) {
    int r = ata_select_lba(lba, 1);
    if (r != ATA_OK) return r;

    outb(ATA_COMMAND, ATA_CMD_WRITE);
    io_wait();

    r = ata_wait_drq();
    if (r != ATA_OK) return r;

    for (int i = 0; i < 256; i++) {
        u16 word = (u16)in[i * 2] | ((u16)in[i * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready();
    return ATA_OK;
}

// ── Magic NovaFS ───────────────────────────────────────────
static int has_novafs_magic(const u8* sector) {
    return sector[0] == 'N' && sector[1] == 'O' && sector[2] == 'V' &&
           sector[3] == 'A' && sector[4] == 'F' && sector[5] == 'S' &&
           sector[6] == '1';
}

// ── Rilevamento disco via IDENTIFY ─────────────────────────
static int ata_detect_disk(void) {
    u8 status = inb(ATA_STATUS);
    // 0xFF = nessun dispositivo sul bus
    // 0x00 = bus floating (nessun disco)
    if (status == 0xFF || status == 0x00) return 0;

    // Seleziona master drive
    outb(ATA_DRIVE, 0xA0);
    io_wait();

    // Invia IDENTIFY
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    status = inb(ATA_STATUS);
    if (status == 0x00) return 0;  // disco non presente

    // Aspetta che BSY si abbassi
    for (u32 i = 0; i < 100000; i++) {
        status = inb(ATA_STATUS);
        if (!(status & ATA_SR_BSY)) break;
    }

    // Se LBA_MID o LBA_HIGH != 0 è ATAPI (CD-ROM), non ATA
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0) return 0;

    // Aspetta DRQ o ERR
    for (u32 i = 0; i < 100000; i++) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return 0;
        if (status & ATA_SR_DRQ) break;
    }

    // Svuota il buffer IDENTIFY (256 words)
    for (int i = 0; i < 256; i++) inw(ATA_DATA);

    return 1;
}

// ============================================================
//  API PUBBLICA
// ============================================================

void storage_init(void) {
    last_error = ATA_OK;
    storage_bus_ready = ata_detect_disk();
}

int storage_ready(void) {
    return storage_bus_ready;
}

int storage_last_error(void) {
    return last_error;
}

const char* storage_status_text(void) {
    if (!storage_bus_ready) return "Storage: no disk detected (safe mode)";
    switch (last_error) {
        case ATA_ERR_TIMEOUT: return "Storage: disk timeout (last op)";
        case ATA_ERR_FAULT:   return "Storage: disk hardware error";
        case ATA_ERR_MAGIC:   return "Storage: NovaFS not formatted";
        default:              return "Storage: ATA disk ready, NovaFS available";
    }
}

int storage_format_novafs(void) {
    if (!storage_bus_ready) { last_error = ATA_ERR_NO_DISK; return 0; }

    u8 sector[512];
    for (int i = 0; i < 512; i++) sector[i] = 0;
    sector[0] = 'N'; sector[1] = 'O'; sector[2] = 'V';
    sector[3] = 'A'; sector[4] = 'F'; sector[5] = 'S';
    sector[6] = '1';
    sector[8] = (u8)NOVAFS_SECTORS;

    if (ata_write_sector(NOVAFS_LBA, sector) != ATA_OK) return 0;
    last_error = ATA_OK;
    return 1;
}

int storage_read_novafs(u8* buffer, int bytes) {
    if (!storage_bus_ready) { last_error = ATA_ERR_NO_DISK; return 0; }
    if (!buffer || bytes > NOVAFS_BYTES) { last_error = ATA_ERR_PARAM; return 0; }

    u8 first[512];
    if (ata_read_sector(NOVAFS_LBA, first) != ATA_OK) return 0;
    if (!has_novafs_magic(first)) { last_error = ATA_ERR_MAGIC; return 0; }

    int pos = 0;
    for (int s = 0; s < NOVAFS_SECTORS && pos < bytes; s++) {
        u8 sector[512];
        if (s == 0) {
            for (int i = 0; i < 512; i++) sector[i] = first[i];
        } else if (ata_read_sector(NOVAFS_LBA + (u32)s, sector) != ATA_OK) {
            return 0;
        }
        for (int i = 0; i < 512 && pos < bytes; i++)
            buffer[pos++] = sector[i];
    }
    last_error = ATA_OK;
    return 1;
}

int storage_write_novafs(const u8* buffer, int bytes) {
    if (!storage_bus_ready) { last_error = ATA_ERR_NO_DISK; return 0; }
    if (!buffer || bytes > NOVAFS_BYTES) { last_error = ATA_ERR_PARAM; return 0; }
    if (!has_novafs_magic(buffer)) { last_error = ATA_ERR_MAGIC; return 0; }

    u8 first[512];
    if (ata_read_sector(NOVAFS_LBA, first) != ATA_OK) return 0;
    if (!has_novafs_magic(first)) { last_error = ATA_ERR_MAGIC; return 0; }

    int pos = 0;
    for (int s = 0; s < NOVAFS_SECTORS; s++) {
        u8 sector[512];
        for (int i = 0; i < 512; i++) {
            sector[i] = pos < bytes ? buffer[pos] : 0;
            pos++;
        }
        if (ata_write_sector(NOVAFS_LBA + (u32)s, sector) != ATA_OK) return 0;
    }
    last_error = ATA_OK;
    return 1;
}
