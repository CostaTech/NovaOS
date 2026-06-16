#include "nova.h"

// Framebuffer placeholder. The first stable NovaOS screen uses VGA text mode.
// This file stays here so graphics can grow without mixing with boot or keyboard.

static int fb_ok = 0;

void framebuffer_init(u32 magic, u32 mbi_addr) {
    (void)magic;
    (void)mbi_addr;
    fb_ok = 0;
}

int framebuffer_ready(void) { return fb_ok; }
void framebuffer_clear(u32 color) { (void)color; }
void framebuffer_rect(int x, int y, int w, int h, u32 color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

void framebuffer_demo(void) {
    if (!fb_ok) return;
    framebuffer_clear(0x00001030);
    framebuffer_rect(20, 20, 180, 80, 0x002040C0);
    framebuffer_rect(40, 45, 140, 30, 0x0060A0FF);
}
