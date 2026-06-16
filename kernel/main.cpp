#include "nova.h"

void desktop_loop();
void ramfs_init();
void tenclelang_init();

extern "C" void kernel_main(u32 magic, u32 mbi_addr) {
    if (magic != 0x2BADB002) {
        NOVA_PANIC("Invalid Multiboot magic. Bootloader did not load NovaOS correctly.");
    }

    framebuffer_init(magic, mbi_addr);
    keyboard_init();
    mouse_init();
    ramfs_init();
    tenclelang_init();

    desktop_loop();

    for (;;) __asm__ volatile ("hlt");
}
