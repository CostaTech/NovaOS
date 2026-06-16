#include "nova.h"

static int storage_bus_ready = 0;

void storage_init(void) {
    // Safe phase: detect the classic ATA status port, but do not write sectors yet.
    u8 status = inb(0x1F7);
    storage_bus_ready = status != 0xFF && status != 0x00;
}

int storage_ready(void) {
    return storage_bus_ready;
}

const char* storage_status_text(void) {
    if (storage_bus_ready) return "Storage bus visible, writes locked";
    return "Storage safe mode, no disk writes";
}
