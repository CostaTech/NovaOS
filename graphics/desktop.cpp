#include "nova.h"

void shell_run();

static void run_novac_app(const char* filename);

static const char* AUTHOR = "CostaTech";
static int selected_app = 0;
static const char dock_actions[10] = {'f', 't', 's', 'c', 'n', 'p', 'd', 'o', 'h', 'm'};

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
    vga_write_at(0, 23, "================================================================================", 0x30);
    vga_write_at(0, 24, "Use UP/DOWN + ENTER to open apps. Mouse click is optional.", 0x3E);
    if (text) vga_write_at(66, 24, text, 0x3B);
}

static void draw_top_bar(void) {
    vga_write_at(0, 0, "================================================================================", 0x30);
    vga_write_at(2, 1, "NOVA OS // 1997", 0x3E);
    vga_write_at(20, 1, "GALACTIC DESKTOP", 0x3B);
    vga_write_at(43, 1, "User:", 0x3A);
    vga_write_at(49, 1, session_username(), 0x3B);
    write_right(76, 1, AUTHOR, 0x3A);
}

static void draw_satellite_logo(int x, int y) {
    vga_write_at(x + 2, y, " .-. ", 0x1B);
    vga_write_at(x, y + 1, "-=[##]=-", 0x1E);
    vga_write_at(x + 2, y + 2, " `-' ", 0x1B);
    vga_write_at(x + 1, y + 3, "NOVA LINK", 0x1A);
}

static void draw_dock_icon(int x, int y, int selected, const char* icon, const char* label, u8 color) {
    u8 frame = selected ? 0x4E : color;
    u8 text = selected ? 0x4F : 0x1F;
    vga_write_at(x - 1, y, selected ? ">" : " ", selected ? 0x4E : 0x10);
    vga_write_at(x, y, "[  ]", frame);
    vga_write_at(x + 1, y, icon, selected ? 0x4F : 0x1E);
    vga_write_at(x + 4, y, label, text);
}

static void draw_stars(void) {
    for (int y = 2; y < 23; y++) {
        for (int x = 1; x < 79; x++) {
            int v = (x * 13 + y * 17) % 83;
            if (v == 0) vga_write_at(x, y, ".", 0x18);
            else if (v == 5) vga_write_at(x, y, "+", 0x19);
            else if (v == 11) vga_write_at(x, y, "-", 0x18);
        }
    }
    vga_write_at(18, 5, ".     .      .", 0x19);
    vga_write_at(19, 6, "\\   /\\    /", 0x18);
    vga_write_at(20, 7, "`-.*--*--.'", 0x19);
    vga_write_at(16, 11, "..--*-----..       .", 0x18);
    vga_write_at(14, 16, "   .-- orbital map --.", 0x19);
}

static void draw_resource_panel(void) {
    box(1, 16, 18, 7, " Resources ", 0x1A);
    vga_write_at(3, 18, "[DOCS]", 0x1E);
    vga_write_at(3, 19, "[APPS]", 0x1B);
    vga_write_at(3, 20, "[HOME]", 0x1F);
    vga_write_at(3, 21, "[IMAGES]", 0x1D);
}

static void draw_calendar_panel(void) {
    box(21, 3, 13, 6, " DATE ", 0x1E);
    vga_write_at(24, 4, "NOVA", 0x1C);
    vga_write_at(25, 5, "16", 0x1F);
    vga_write_at(23, 6, "JUN 2026", 0x1E);
    vga_write_at(24, 7, "TUE", 0x1B);
}

static void draw_note_window(void) {
    box(36, 4, 41, 15, " Nova-Writer // Notes ", 0x1E);
    vga_write_at(38, 6, "Welcome to NOVA OS.", 0x1F);
    vga_write_at(38, 8, "Explore files, apps and NovaC.", 0x1F);
    vga_write_at(38, 10, "Default apps are protected.", 0x1A);
    vga_write_at(38, 12, "Today: build the storage layer.", 0x1B);
    vga_write_at(38, 15, "Open apps from the left dock.", 0x1E);
    vga_write_at(38, 16, "NovaC apps run inside NovaOS.", 0x1D);
}

static void draw_system_panel(void) {
    box(59, 18, 20, 5, " System ", 0x1A);
    vga_write_at(61, 19, "MEM [48 MB]", 0x1E);
    if (storage_ready()) vga_write_at(61, 20, "DISK visible", 0x1A);
    else vga_write_at(61, 20, "DISK safe", 0x1E);
    if (mouse_enabled()) vga_write_at(61, 21, "MOUSE filtered", 0x1B);
    else vga_write_at(61, 21, "MOUSE off", 0x1C);
}

void desktop_draw() {
    vga_clear(0x10);
    draw_stars();

    draw_top_bar();
    draw_satellite_logo(68, 3);

    box(1, 3, 18, 12, " Apps ", 0x1A);
    draw_dock_icon(3, 4, selected_app == 0, "/", "Files", 0x1B);
    draw_dock_icon(3, 5, selected_app == 1, ">", "Terminal", 0x1A);
    draw_dock_icon(3, 6, selected_app == 2, "#", "Settings", 0x1D);
    draw_dock_icon(3, 7, selected_app == 3, "+", "Calc", 0x1E);
    draw_dock_icon(3, 8, selected_app == 4, "=", "Notes", 0x1B);
    draw_dock_icon(3, 9, selected_app == 5, "*", "Paint", 0x1D);
    draw_dock_icon(3, 10, selected_app == 6, "?", "Docs", 0x1E);
    draw_dock_icon(3, 11, selected_app == 7, "@", "Tour", 0x1A);
    draw_dock_icon(3, 12, selected_app == 8, "!", "Hello", 0x1F);
    draw_dock_icon(3, 13, selected_app == 9, "^", "Games", 0x1D);

    draw_calendar_panel();
    draw_note_window();
    draw_resource_panel();
    draw_system_panel();

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
    vga_write_at(55, 1, "Text-mode graphic app", 0x0B);

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
    ramfs_cd("/");
    ramfs_cd("games");
    files_app();
}

static void about_app() {
    vga_clear(0x1F);
    vga_write_at(2, 1, "NovaOS / About", 0x1E);
    box(2, 3, 74, 17, " About NovaOS ", 0x1E);
    vga_write_at(4, 5, "NovaOS", 0x1E);
    vga_write_at(4, 7, "A new operating system built with Assembly, NovaC, C and C++.", 0x1F);
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

static void run_novac_app(const char* filename) {
    ramfs_cd("/");
    ramfs_cd("apps");

    const char* source = ramfs_read_file(filename);

    vga_clear(0x0F);
    vga_write_at(2, 1, "NovaOS / NovaC App", 0x0E);
    vga_write_at(2, 2, filename, 0x0B);

    if (!source) {
        vga_write_at(2, 4, "App not found.", 0x0C);
    } else {
        vga_set_color(0x0F);
        novac_run_source(source);
    }

    vga_write_at(2, 22, "Press ESC to return to desktop.", 0x0E);
    wait_escape_then_desktop();
}

static int inside_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static char icon_at(int x, int y) {
    if (inside_rect(x, y, 3, 4, 14, 1)) return 'f';
    if (inside_rect(x, y, 3, 5, 14, 1)) return 't';
    if (inside_rect(x, y, 3, 6, 14, 1)) return 's';
    if (inside_rect(x, y, 3, 7, 14, 1)) return 'c';
    if (inside_rect(x, y, 3, 8, 14, 1)) return 'n';
    if (inside_rect(x, y, 3, 9, 14, 1)) return 'p';
    if (inside_rect(x, y, 3, 10, 14, 1)) return 'd';
    if (inside_rect(x, y, 3, 11, 14, 1)) return 'o';
    if (inside_rect(x, y, 3, 12, 14, 1)) return 'h';
    if (inside_rect(x, y, 3, 13, 14, 1)) return 'm';
    if (inside_rect(x, y, 59, 18, 20, 5)) return 's';
    if (inside_rect(x, y, 36, 4, 41, 15)) return 'a';
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
        run_novac_app("settings.nc");
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
    } else if (action == 'c' || action == 'C') {
        run_novac_app("calculator.nc");
        desktop_redraw();
    } else if (action == 'n' || action == 'N') {
        run_novac_app("notes.nc");
        desktop_redraw();
    } else if (action == 'p' || action == 'P') {
        run_novac_app("paint.nc");
        desktop_redraw();
    } else if (action == 'd' || action == 'D') {
        run_novac_app("documentation.nc");
        desktop_redraw();
    } else if (action == 'o' || action == 'O') {
        run_novac_app("tour.nc");
        desktop_redraw();
    } else if (action == 'h' || action == 'H') {
        run_novac_app("hello.nc");
        desktop_redraw();
    } else if (action == 'r' || action == 'R') {
        if (confirm_action(" Reboot ", "Do you want to reboot NovaOS?")) system_reboot();
        desktop_redraw();
    }
}

void desktop_loop() {
    desktop_redraw();
    int last_x = mouse_x();
    int last_y = mouse_y();
    int last_buttons = mouse_buttons();
    int last_left_down = mouse_buttons() & 1;
    int press_x = 0;
    int press_y = 0;
    char press_action = 0;
    int idle_ticks = 0;

    for (;;) {
        mouse_poll();
        idle_ticks++;

        int left_down = mouse_buttons() & 1;
        if (left_down && !last_left_down) {
            press_x = mouse_x();
            press_y = mouse_y();
            press_action = icon_at(press_x, press_y);
            if (press_action == 'r') press_action = 0;
        }

        if (!left_down && last_left_down) {
            char release_action = icon_at(mouse_x(), mouse_y());
            int moved_x = mouse_x() - press_x;
            int moved_y = mouse_y() - press_y;
            if (moved_x < 0) moved_x = -moved_x;
            if (moved_y < 0) moved_y = -moved_y;

            if (press_action && release_action == press_action && moved_x <= 1 && moved_y <= 1) {
                open_desktop_action(press_action);
                last_x = mouse_x();
                last_y = mouse_y();
                last_buttons = mouse_buttons();
                last_left_down = mouse_buttons() & 1;
                press_action = 0;
                continue;
            }
            press_action = 0;
        }

        last_left_down = left_down;

        if ((mouse_x() != last_x || mouse_y() != last_y || mouse_buttons() != last_buttons) && idle_ticks > 800) {
            last_x = mouse_x();
            last_y = mouse_y();
            last_buttons = mouse_buttons();
            idle_ticks = 0;
            desktop_redraw();
        }

        char key = keyboard_try_read_char();
        if (key == NOVA_KEY_UP) {
            selected_app--;
            if (selected_app < 0) selected_app = 9;
            desktop_redraw();
            last_x = mouse_x();
            last_y = mouse_y();
            last_buttons = mouse_buttons();
            idle_ticks = 0;
        } else if (key == NOVA_KEY_DOWN) {
            selected_app++;
            if (selected_app > 9) selected_app = 0;
            desktop_redraw();
            last_x = mouse_x();
            last_y = mouse_y();
            last_buttons = mouse_buttons();
            idle_ticks = 0;
        } else if (key == '\n') {
            open_desktop_action(dock_actions[selected_app]);
            desktop_redraw();
            last_x = mouse_x();
            last_y = mouse_y();
            last_buttons = mouse_buttons();
            idle_ticks = 0;
        }
    }
}
