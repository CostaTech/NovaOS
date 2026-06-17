#include "nova.h"

#define NOVA_ENABLE_MOUSE 1

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define MOUSE_MAX_PACKETS_PER_POLL 8
#define MOUSE_MAX_DELTA 18
#define MOUSE_TEXT_STEP 7

static int mx = 40;
static int my = 12;
static int mb = 0;
static int mouse_ready = 0;
static int accum_x = 0;
static int accum_y = 0;

#if NOVA_ENABLE_MOUSE
static u8 packet[3];
static int packet_index = 0;

static int ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return 1;
    }
    return 0;
}

static int ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) return 1;
    }
    return 0;
}

static void ps2_write_command(u8 value) {
    if (ps2_wait_write()) outb(PS2_COMMAND, value);
}

static void ps2_write_data(u8 value) {
    if (ps2_wait_write()) outb(PS2_DATA, value);
}

static u8 ps2_read_data(void) {
    if (!ps2_wait_read()) return 0;
    return inb(PS2_DATA);
}

static void ps2_flush_output(void) {
    for (int i = 0; i < 64; i++) {
        if ((inb(PS2_STATUS) & 0x01) == 0) return;
        (void)inb(PS2_DATA);
    }
}

static int mouse_write(u8 value) {
    ps2_write_command(0xD4);
    ps2_write_data(value);
    return ps2_read_data() == 0xFA;
}

static void clamp_position(void) {
    if (mx < 1) mx = 1;
    if (my < 1) my = 1;
    if (mx > 78) mx = 78;
    if (my > 23) my = 23;
}

static int abs_int(int value) {
    return value < 0 ? -value : value;
}
#endif

void mouse_init(void) {
    mx = 40;
    my = 12;
    mb = 0;
    mouse_ready = 0;
    accum_x = 0;
    accum_y = 0;

#if NOVA_ENABLE_MOUSE
    packet_index = 0;
    ps2_flush_output();

    ps2_write_command(0xA8);
    ps2_write_command(0x20);
    u8 config = ps2_read_data();
    config |= 0x02;
    config &= (u8)~0x20;
    ps2_write_command(0x60);
    ps2_write_data(config);

    if (!mouse_write(0xF6)) return; // defaults
    if (!mouse_write(0xE6)) return; // 1:1 scaling, safer than acceleration
    if (!mouse_write(0xE8)) return; // resolution command
    if (!mouse_write(0x00)) return; // lowest resolution: calmer real hardware
    if (!mouse_write(0xF3)) return; // sample rate command
    if (!mouse_write(20)) return;   // 20 samples/sec for text-mode desktop
    if (!mouse_write(0xF4)) return; // enable streaming

    ps2_flush_output();
    mouse_ready = 1;
#endif
}

void mouse_poll(void) {
#if NOVA_ENABLE_MOUSE
    if (!mouse_ready) return;

    int packets_seen = 0;
    while ((inb(PS2_STATUS) & 0x01) && packets_seen < MOUSE_MAX_PACKETS_PER_POLL) {
        u8 status = inb(PS2_STATUS);

        if ((status & 0x20) == 0) {
            packet_index = 0;
            break;
        }

        u8 data = inb(PS2_DATA);
        if (packet_index == 0 && (data & 0xC8) != 0x08) continue;
        packet[packet_index++] = data;

        if (packet_index < 3) continue;
        packet_index = 0;
        packets_seen++;

        if (packet[0] & 0x40) continue; // X overflow: ignore bad packet.
        if (packet[0] & 0x80) continue; // Y overflow: ignore bad packet.

        int dx = (int)packet[1];
        int dy = (int)packet[2];
        if (packet[0] & 0x10) dx -= 256;
        if (packet[0] & 0x20) dy -= 256;

        if (abs_int(dx) > MOUSE_MAX_DELTA || abs_int(dy) > MOUSE_MAX_DELTA) continue;

        accum_x += dx;
        accum_y += dy;

        while (accum_x >= MOUSE_TEXT_STEP) { mx++; accum_x -= MOUSE_TEXT_STEP; }
        while (accum_x <= -MOUSE_TEXT_STEP) { mx--; accum_x += MOUSE_TEXT_STEP; }
        while (accum_y >= MOUSE_TEXT_STEP) { my--; accum_y -= MOUSE_TEXT_STEP; }
        while (accum_y <= -MOUSE_TEXT_STEP) { my++; accum_y += MOUSE_TEXT_STEP; }

        mb = packet[0] & 0x07;
        clamp_position();
    }
#endif
}

int mouse_x(void) { return mx; }
int mouse_y(void) { return my; }
int mouse_buttons(void) { return mb; }
int mouse_enabled(void) { return NOVA_ENABLE_MOUSE; }
int mouse_is_ready(void) { return mouse_ready; }
