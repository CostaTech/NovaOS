#include "nova.h"

void desktop_loop();
void ramfs_init();
void tenclelang_init();

static void login_screen() {
    char username[32];
    int len = 0;
    username[0] = 0;

    vga_clear(0x10);
    vga_write_at(24, 5, "+------------------------------+", 0x1E);
    vga_write_at(24, 6, "|          NovaOS Login        |", 0x1E);
    vga_write_at(24, 7, "+------------------------------+", 0x1E);
    vga_write_at(26, 10, "Username:", 0x1B);
    vga_write_at(26, 13, "ENTER starts NovaOS", 0x1A);
    vga_write_at(26, 14, "Password support comes with storage.", 0x18);
    vga_write_at(36, 10, "_______________________________", 0x18);

    for (;;) {
        char c = keyboard_read_char();
        if (c == '\n') {
            if (len == 0) {
                username[0] = 'g';
                username[1] = 'u';
                username[2] = 'e';
                username[3] = 's';
                username[4] = 't';
                username[5] = 0;
            }
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                username[len] = 0;
                vga_write_at(36, 10, "_______________________________", 0x18);
                vga_write_at(36, 10, username, 0x1F);
            }
        } else if (c >= 32 && c <= 126 && len < 31) {
            username[len++] = c;
            username[len] = 0;
            vga_write_at(36, 10, "_______________________________", 0x18);
            vga_write_at(36, 10, username, 0x1F);
        }
    }

    vga_clear(0x10);
    vga_write_at(26, 10, "Welcome to NovaOS,", 0x1E);
    vga_write_at(45, 10, username, 0x1B);
    for (int i = 0; i < 8000000; i++) __asm__ volatile ("nop");
}

extern "C" void kernel_main(u32 magic, u32 mbi_addr) {
    if (magic != 0x2BADB002) {
        NOVA_PANIC("Invalid Multiboot magic. Bootloader did not load NovaOS correctly.");
    }

    framebuffer_init(magic, mbi_addr);
    keyboard_init();
    mouse_init();
    ramfs_init();
    tenclelang_init();

    login_screen();
    desktop_loop();

    for (;;) __asm__ volatile ("hlt");
}
