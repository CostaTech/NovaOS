#include "nova.h"

static void wait_input_buffer_empty() {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

extern "C" void system_reboot(void) {
    vga_clear(0x0E);
    vga_write_at(2, 2, "NovaOS is rebooting...", 0x0E);

    __asm__ volatile ("cli");
    wait_input_buffer_empty();
    outb(0x64, 0xFE); // 8042 CPU reset pulse.

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

extern "C" void system_shutdown(void) {
    vga_clear(0x0F);
    vga_write_at(2, 2, "NovaOS shutdown", 0x0E);
    vga_write_at(2, 4, "Trying emulator/ACPI-compatible power off...", 0x0F);

    // QEMU/Bochs common poweroff ports. Real PCs usually need ACPI tables,
    // so if these do not work we fall back to a safe halt screen.
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    vga_clear(0x0F);
    vga_write_at(2, 2, "NovaOS is halted.", 0x0E);
    vga_write_at(2, 4, "It is now safe to power off this computer.", 0x0F);
    vga_write_at(2, 6, "If you are in QEMU/VirtualBox and it did not close,", 0x07);
    vga_write_at(2, 7, "shutdown support for this machine is not available yet.", 0x07);

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
