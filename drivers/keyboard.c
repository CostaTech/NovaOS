#include "nova.h"

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

static int shift_down = 0;
static int caps_lock = 0;
static int extended_key = 0;

static const char keymap[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'', '`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

static const char keymap_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"', '~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0
};

static int is_letter(char c) {
    return c >= 'a' && c <= 'z';
}

static char to_upper(char c) {
    if (is_letter(c)) return (char)(c - 'a' + 'A');
    return c;
}

void keyboard_init(void) {
    shift_down = 0;
    caps_lock = 0;
    extended_key = 0;
    while (inb(KBD_STATUS) & 1) (void)inb(KBD_DATA);
}

char keyboard_try_read_char(void) {
    if ((inb(KBD_STATUS) & 1) == 0) return 0;

    u8 status = inb(KBD_STATUS);
    u8 sc = inb(KBD_DATA);

    if (status & 0x20) return 0;       // AUX/mouse byte: ignore, keyboard only.

    if (sc == 0xE0) {
        extended_key = 1;
        return 0;
    }

    if (extended_key) {
        extended_key = 0;
        if (sc & 0x80) return 0;
        if (sc == 0x1C) return '\n';
        if (sc == 0x48) return NOVA_KEY_UP;
        if (sc == 0x50) return NOVA_KEY_DOWN;
        if (sc == 0x4B) return NOVA_KEY_LEFT;
        if (sc == 0x4D) return NOVA_KEY_RIGHT;
        return 0;
    }

    // Key release.
    if (sc & 0x80) {
        u8 released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) shift_down = 0;
        return 0;
    }

    // Left/right Shift press.
    if (sc == 0x2A || sc == 0x36) {
        shift_down = 1;
        return 0;
    }

    // Caps Lock press.
    if (sc == 0x3A) {
        caps_lock = !caps_lock;
        return 0;
    }

    // Some BIOS/QEMU setups send arrow keys without the 0xE0 prefix.
    if (sc == 0x48) return NOVA_KEY_UP;
    if (sc == 0x50) return NOVA_KEY_DOWN;
    if (sc == 0x4B) return NOVA_KEY_LEFT;
    if (sc == 0x4D) return NOVA_KEY_RIGHT;

    if (sc < 128) {
        char c = shift_down ? keymap_shift[sc] : keymap[sc];
        if (!shift_down && caps_lock && is_letter(c)) c = to_upper(c);
        if (shift_down && caps_lock && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        return c;
    }

    return 0;
}

char keyboard_read_char(void) {
    for (;;) {
        char c = keyboard_try_read_char();
        if (c) return c;
    }
}
