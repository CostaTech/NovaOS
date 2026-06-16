#include "nova.h"

void shell_run();

static const char* AUTHOR = "CostaTech";

static int text_len(const char* text) {
    int n = 0;
    while (text && text[n]) n++;
    return n;
}

static void write_right(int right_col, int y, const char* text, u8 color) {
    int x = right_col - text_len(text) + 1;
    if (x < 0) x = 0;
    vga_write_at(x, y, text, color);
}

static void box(int x, int y, int w, int h, const char* title, u8 color) {
    for (int i = 0; i < w; i++) {
        vga_write_at(x + i, y, "-", color);
        vga_write_at(x + i, y + h - 1, "-", color);
    }
    for (int j = 0; j < h; j++) {
        vga_write_at(x, y + j, "|", color);
        vga_write_at(x + w - 1, y + j, "|", color);
    }
    vga_write_at(x, y, "+", color);
    vga_write_at(x + w - 1, y, "+", color);
    vga_write_at(x, y + h - 1, "+", color);
    vga_write_at(x + w - 1, y + h - 1, "+", color);
    if (title) vga_write_at(x + 2, y, title, color);
}

static void draw_footer(const char* text) {
    vga_write_at(0, 24, "                                                                                ", 0x30);
    vga_write_at(2, 24, "F Files", 0x3E);
    vga_write_at(12, 24, "T Terminal", 0x3B);
    vga_write_at(25, 24, "S Settings", 0x3D);
    vga_write_at(38, 24, "A About", 0x3A);
    vga_write_at(49, 24, "R Reboot", 0x3C);
    vga_write_at(61, 24, "P Shutdown", 0x3C);
    vga_write_at(73, 24, "M Games", 0x3D);
    if (text) vga_write_at(68, 1, text, 0x1E);
}

static void draw_satellite_logo(int x, int y) {
    vga_write_at(x + 2, y, " .-. ", 0x1B);
    vga_write_at(x, y + 1, "-=[##]=-", 0x1E);
    vga_write_at(x + 2, y + 2, " `-' ", 0x1B);
    vga_write_at(x + 1, y + 3, "NOVA LINK", 0x1A);
}

static void draw_icon(int x, int y, const char* key, const char* art1, const char* art2, const char* label, u8 color) {
    vga_write_at(x, y, "+--------+", color);
    vga_write_at(x, y + 1, "|        |", color);
    vga_write_at(x, y + 2, "|        |", color);
    vga_write_at(x, y + 3, "+--------+", color);
    vga_write_at(x + 2, y + 1, art1, color);
    vga_write_at(x + 2, y + 2, art2, color);
    vga_write_at(x + 1, y + 4, key, 0x1F);
    vga_write_at(x + 4, y + 4, label, 0x1F);
}

static void draw_stars(void) {
    for (int y = 3; y < 23; y++) {
        for (int x = 1; x < 79; x++) {
            int v = (x * 13 + y * 17) % 71;
            if (v == 0) vga_write_at(x, y, ".", 0x18);
            else if (v == 5) vga_write_at(x, y, "+", 0x19);
        }
    }
}

void desktop_draw() {
    vga_clear(0x10);
    draw_stars();

    vga_write_at(0, 0, "                                                                                ", 0x30);
    vga_write_at(2, 1, "NOVAOS // DESKTOP", 0x3E);
    write_right(63, 1, AUTHOR, 0x3A);
    draw_satellite_logo(67, 3);

    box(2, 3, 60, 15, " Nova Workspace ", 0x1E);
    draw_icon(5, 5, "F", " /--", "[__]", "Files", 0x1B);
    draw_icon(18, 5, "T", ">__", "|__", "Term", 0x1A);
    draw_icon(31, 5, "S", "{##", " ##", "Set", 0x1D);
    draw_icon(44, 5, "A", "NVA", " OS", "About", 0x1E);

    draw_icon(5, 12, "G", ".*.", "***", "Galaxy", 0x1D);
    draw_icon(18, 12, "R", "[>>", " >>", "Reboot", 0x1C);
    draw_icon(31, 12, "P", "[--", " --", "Power", 0x1C);
    draw_icon(44, 12, "M", "[<>]", "PLAY", "Games", 0x1D);

    box(64, 9, 14, 9, " Status ", 0x1A);
    vga_write_at(66, 11, "Kernel OK", 0x1A);
    vga_write_at(66, 12, "NovaFS RAM", 0x1B);
    if (mouse_enabled()) vga_write_at(66, 13, "Mouse ON", 0x1A);
    else vga_write_at(66, 13, "Mouse OFF", 0x1C);
    vga_write_at(66, 15, "TencleLang", 0x1E);

    draw_footer("Ready");
}

static void draw_mouse_cursor(void) {
    if (!mouse_enabled() || !mouse_is_ready()) return;
    vga_write_at(mouse_x(), mouse_y(), ">", 0x4F);
}

static void desktop_redraw(void) {
    desktop_draw();
    draw_mouse_cursor();
}


static void files_app() {
    vga_clear(0x1F);
    vga_write_at(2, 1, "NovaOS / Files", 0x1E);
    box(2, 3, 74, 17, " File Manager ", 0x1E);
    vga_write_at(4, 5, "Current folder:", 0x1F);
    vga_write_at(20, 5, ramfs_pwd(), 0x1B);

    int count = ramfs_child_count();
    if (count == 0) {
        vga_write_at(4, 7, "This folder is empty.", 0x1F);
    }

    for (int i = 0; i < count && i < 10; i++) {
        int y = 7 + i;
        if (ramfs_child_is_dir(i)) vga_write_at(4, y, "[DIR] ", 0x1E);
        else vga_write_at(4, y, "[FILE]", 0x1B);
        vga_write_at(12, y, ramfs_child_name(i), 0x1F);
    }

    vga_write_at(4, 18, "Use Terminal for cd, mkdir, create, cat and rm.", 0x1E);
    vga_write_at(4, 19, "Press ESC to return to desktop.", 0x1E);
}


static void galaxy_app() {
    vga_clear(0x0F);
    vga_write_at(2, 1, "NovaOS / Galaxy", 0x0E);
    vga_write_at(55, 1, "Text-mode graphic demo", 0x0B);

    for (int y = 3; y < 22; y++) {
        for (int x = 2; x < 78; x++) {
            int v = (x * 7 + y * 11) % 37;
            if (v == 0) vga_write_at(x, y, ".", 0x0B);
            else if (v == 3) vga_write_at(x, y, "*", 0x0F);
            else if (v == 9) vga_write_at(x, y, "+", 0x0D);
        }
    }

    box(20, 7, 40, 9, " Nova Station ", 0x0E);
    vga_write_at(25, 10, "NovaOS graphic layer will grow here.", 0x1F);
    vga_write_at(25, 12, "Next: framebuffer, windows, icons.", 0x1B);
    vga_write_at(25, 14, "Press ESC to return.", 0x1E);
}


static void games_app() {
    vga_clear(0x1F);
    vga_write_at(2, 1, "NovaOS / Games", 0x1E);
    box(2, 3, 74, 17, " Games Launcher ", 0x1E);
    vga_write_at(4, 5, "Games are planned as TencleLang apps.", 0x1F);
    vga_write_at(4, 7, "Folder:", 0x1A);
    vga_write_at(13, 7, "/games", 0x1B);
    vga_write_at(4, 9, "Try in Terminal:", 0x1E);
    vga_write_at(6, 11, "cd /", 0x1F);
    vga_write_at(6, 12, "cd games", 0x1F);
    vga_write_at(6, 13, "tlrun hello_game.tlang", 0x1F);
    vga_write_at(4, 18, "Press ESC to return to desktop.", 0x1E);
}

static void settings_app() {
    vga_clear(0x1F);
    vga_write_at(2, 1, "NovaOS / Settings", 0x1E);
    box(2, 3, 74, 17, " Settings ", 0x1E);
    vga_write_at(4, 5, "Theme: Nova Blue", 0x1F);
    vga_write_at(4, 6, "Input: keyboard stable, mouse isolated", 0x1F);
    vga_write_at(4, 7, "Boot: GRUB Multiboot", 0x1F);
    vga_write_at(4, 8, "Images: planned .lnp Nova Picture", 0x1F);
    vga_write_at(4, 9, "Mouse: isolated driver, real packets later", 0x1F);
    vga_write_at(4, 18, "Press ESC to return to desktop.", 0x1E);
}

static void about_app() {
    vga_clear(0x1F);
    vga_write_at(2, 1, "NovaOS / About", 0x1E);
    box(2, 3, 74, 17, " About NovaOS ", 0x1E);
    vga_write_at(4, 5, "NovaOS", 0x1E);
    vga_write_at(4, 7, "A new operating system built with Assembly, TencleLang, C and C++.", 0x1F);
    vga_write_at(4, 8, "Created by CostaTech.", 0x0C);
    vga_write_at(4, 10, "Press ESC to return to desktop.", 0x1E);
}

static int confirm_action(const char* title, const char* question) {
    vga_clear(0x1F);
    box(18, 7, 44, 9, title, 0x1E);
    vga_write_at(21, 10, question, 0x1F);
    vga_write_at(21, 12, "Press Y to confirm or N/ESC to cancel.", 0x1E);

    for (;;) {
        char c = keyboard_read_char();
        if (c == 'y' || c == 'Y') return 1;
        if (c == 'n' || c == 'N' || c == 27) return 0;
    }
}

static void wait_escape_then_desktop() {
    for (;;) {
        char c = keyboard_read_char();
        if (c == 27) return;
    }
}

static int inside_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static char icon_at(int x, int y) {
    if (inside_rect(x, y, 5, 5, 10, 5)) return 'f';
    if (inside_rect(x, y, 18, 5, 10, 5)) return 't';
    if (inside_rect(x, y, 31, 5, 10, 5)) return 's';
    if (inside_rect(x, y, 44, 5, 10, 5)) return 'a';
    if (inside_rect(x, y, 5, 12, 10, 5)) return 'g';
    if (inside_rect(x, y, 18, 12, 10, 5)) return 'r';
    if (inside_rect(x, y, 31, 12, 10, 5)) return 'p';
    if (inside_rect(x, y, 44, 12, 10, 5)) return 'm';
    return 0;
}

static void open_desktop_action(char action) {
    if (action == 't' || action == 'T') {
        shell_run();
        desktop_redraw();
    } else if (action == 'f' || action == 'F') {
        files_app();
        wait_escape_then_desktop();
        desktop_redraw();
    } else if (action == 's' || action == 'S') {
        settings_app();
        wait_escape_then_desktop();
        desktop_redraw();
    } else if (action == 'g' || action == 'G') {
        galaxy_app();
        wait_escape_then_desktop();
        desktop_redraw();
    } else if (action == 'a' || action == 'A') {
        about_app();
        wait_escape_then_desktop();
        desktop_redraw();
    } else if (action == 'm' || action == 'M') {
        games_app();
        wait_escape_then_desktop();
        desktop_redraw();
    } else if (action == 'r' || action == 'R') {
        if (confirm_action(" Reboot ", "Do you want to reboot NovaOS?")) system_reboot();
        desktop_redraw();
    } else if (action == 'p' || action == 'P') {
        if (confirm_action(" Shutdown ", "Do you want to shutdown NovaOS?")) system_shutdown();
        desktop_redraw();
    }
}

void desktop_loop() {
    desktop_redraw();
    int last_x = mouse_x();
    int last_y = mouse_y();
    int last_buttons = mouse_buttons();
    int left_stable_ticks = 0;
    int click_armed = 1;
    int idle_ticks = 0;

    for (;;) {
        mouse_poll();
        idle_ticks++;

        int left_down = mouse_buttons() & 1;
        if (left_down) {
            if (left_stable_ticks < 12) left_stable_ticks++;
        } else {
            left_stable_ticks = 0;
            click_armed = 1;
        }

        if (left_down && click_armed && left_stable_ticks >= 3) {
            char action = icon_at(mouse_x(), mouse_y());
            if (action == 'r' || action == 'p') action = 0;
            if (action) {
                click_armed = 0;
                open_desktop_action(action);
                last_x = mouse_x();
                last_y = mouse_y();
                last_buttons = mouse_buttons();
                continue;
            }
        }

        if ((mouse_x() != last_x || mouse_y() != last_y || mouse_buttons() != last_buttons) && idle_ticks > 800) {
            last_x = mouse_x();
            last_y = mouse_y();
            last_buttons = mouse_buttons();
            idle_ticks = 0;
            desktop_redraw();
        }

        char c = keyboard_try_read_char();
        if (c) open_desktop_action(c);
    }
}
