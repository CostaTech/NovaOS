#include "nova.h"

// Mouse is separate from keyboard from day one.
// For now it is safely disabled; later we will implement PS/2 mouse packets here,
// never inside keyboard_read_char().

static int mx = 40;
static int my = 12;
static int mb = 0;

void mouse_init(void) {
    mx = 40;
    my = 12;
    mb = 0;
}

void mouse_poll(void) {
    // TODO(Costa): real mouse driver goes here, separated from keyboard.
}

int mouse_x(void) { return mx; }
int mouse_y(void) { return my; }
int mouse_buttons(void) { return mb; }
