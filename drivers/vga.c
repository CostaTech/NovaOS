#include "nova.h"

#define VGA ((volatile u16*)0xB8000)
#define VGA_W 80
#define VGA_H 25

static int cur_x = 0;
static int cur_y = 0;
static u8 cur_color = 0x0F;

static void scroll_if_needed(void) {
    if (cur_y < VGA_H) return;
    for (int y = 1; y < VGA_H; y++) {
        for (int x = 0; x < VGA_W; x++) VGA[(y - 1) * VGA_W + x] = VGA[y * VGA_W + x];
    }
    for (int x = 0; x < VGA_W; x++) VGA[(VGA_H - 1) * VGA_W + x] = ((u16)cur_color << 8) | ' ';
    cur_y = VGA_H - 1;
}

void vga_set_color(u8 color) { cur_color = color; }

void vga_clear(u8 color) {
    cur_color = color;
    for (int y = 0; y < VGA_H; y++) {
        for (int x = 0; x < VGA_W; x++) VGA[y * VGA_W + x] = ((u16)color << 8) | ' ';
    }
    cur_x = 0;
    cur_y = 0;
}

void vga_putc(char c) {
    if (c == '\n') { cur_x = 0; cur_y++; scroll_if_needed(); return; }
    if (c == '\b') {
        if (cur_x > 0) cur_x--;
        VGA[cur_y * VGA_W + cur_x] = ((u16)cur_color << 8) | ' ';
        return;
    }
    VGA[cur_y * VGA_W + cur_x] = ((u16)cur_color << 8) | (u8)c;
    cur_x++;
    if (cur_x >= VGA_W) { cur_x = 0; cur_y++; scroll_if_needed(); }
}

void vga_write(const char* text) {
    while (*text) vga_putc(*text++);
}

void vga_writeln(const char* text) {
    vga_write(text);
    vga_putc('\n');
}

void vga_write_at(int x, int y, const char* text, u8 color) {
    if (y < 0 || y >= VGA_H) return;
    int i = 0;
    while (text[i] && x + i < VGA_W) {
        if (x + i >= 0) VGA[y * VGA_W + x + i] = ((u16)color << 8) | (u8)text[i];
        i++;
    }
}
