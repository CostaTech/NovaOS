#include "nova.h"

static void print_decimal(int value) {
    char buf[12];
    int i = 0;
    if (value == 0) {
        vga_putc('0');
        return;
    }
    if (value < 0) {
        vga_putc('-');
        value = -value;
    }
    while (value > 0 && i < 11) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) vga_putc(buf[--i]);
}

extern "C" void kernel_panic(const char* message, const char* file, int line) {
    __asm__ volatile ("cli");

    vga_clear(0x4F);
    vga_write_at(2, 2, "NOVAOS KERNEL PANIC", 0x4F);
    vga_write_at(2, 4, "The system stopped to protect itself.", 0x4F);

    vga_write_at(2, 7, "Reason:", 0x4E);
    vga_write_at(10, 7, message ? message : "Unknown panic", 0x4F);

    vga_write_at(2, 9, "File:", 0x4E);
    vga_write_at(8, 9, file ? file : "unknown", 0x4F);

    vga_write_at(2, 10, "Line:", 0x4E);
    vga_set_color(0x4F);
    print_decimal(line);

    vga_write_at(2, 13, "Restart the VM/PC to continue.", 0x4F);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
