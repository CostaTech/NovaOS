#include "nova.h"

static char line[128];
static int line_len = 0;
static int should_exit = 0;
static char last_command[128];

static int streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static int ends_with(const char* text, const char* suffix) {
    int text_len = 0;
    int suffix_len = 0;
    while (text && text[text_len]) text_len++;
    while (suffix && suffix[suffix_len]) suffix_len++;
    if (suffix_len > text_len) return 0;
    return streq(text + text_len - suffix_len, suffix);
}

static const char* skip_spaces(const char* text) {
    while (*text == ' ') text++;
    return text;
}

static int has_arg(const char* arg) {
    return arg && arg[0] != 0;
}

static void copy_text(char* dst, const char* src, int max) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void make_novac_filename(char* dst, const char* name, int max) {
    copy_text(dst, skip_spaces(name), max);
    if (ends_with(dst, ".nc")) return;

    int len = 0;
    while (dst[len]) len++;
    const char* ext = ".nc";
    for (int i = 0; ext[i] && len < max - 1; i++) dst[len++] = ext[i];
    dst[len] = 0;
}

static void terminal_screen() {
    vga_clear(0x0F);
    vga_write_at(2, 1, "NovaOS Terminal", 0x0E);
    vga_write_at(2, 2, "Command deck ready. Type help.", 0x0B);
    vga_write_at(58, 1, "  .-.", 0x0B);
    vga_write_at(58, 2, "-=[#]=-", 0x0E);
    vga_write_at(58, 3, "  `-'", 0x0B);
    vga_write_at(2, 4, "------------------------------------------------------------", 0x08);
    vga_set_color(0x0A);
    vga_write("\nNovaFS:");
    vga_set_color(0x0B);
    vga_write(ramfs_pwd());
    vga_set_color(0x0A);
    vga_write(" > ");
    vga_set_color(0x0F);
}

static void prompt() {
    vga_set_color(0x0A);
    vga_write("Nova:");
    vga_set_color(0x0B);
    vga_write(ramfs_pwd());
    vga_set_color(0x0A);
    vga_write(" > ");
    vga_set_color(0x0F);
}

static void help_line(const char* cmd, const char* text) {
    vga_set_color(0x0B);
    vga_write("  ");
    vga_write(cmd);
    vga_set_color(0x07);
    vga_write(" - ");
    vga_set_color(0x0F);
    vga_writeln(text);
}

static void print_help_index() {
    vga_set_color(0x0E);
    vga_writeln("+------------- NovaOS Help Index -------------+");
    vga_set_color(0x0F);
    help_line("help core", "base terminal commands");
    help_line("help fs", "filesystem commands");
    help_line("help lang", "NovaC commands");
    help_line("help apps", "desktop apps and shortcuts");
    help_line("help input", "keyboard and mouse info");
    help_line("help power", "reboot and shutdown");
    vga_set_color(0x0E);
    vga_writeln("+---------------------------------------------+");
    vga_set_color(0x0F);
}

static void print_help_core() {
    vga_set_color(0x0A);
    vga_writeln("Core commands");
    help_line("help", "show help categories");
    help_line("help <category>", "show one help category");
    help_line("about", "show system information");
    help_line("storage", "show safe storage status");
    help_line("clear", "clear the terminal screen");
    help_line("again", "repeat last command");
    help_line("desktop", "return to the desktop");
}

static void print_help_fs() {
    vga_set_color(0x0D);
    vga_writeln("Filesystem commands");
    help_line("pwd", "show current folder");
    help_line("ls", "list files and folders");
    help_line("tree", "show folder tree");
    help_line("cd <dir>", "enter folder, use cd .. or cd /");
    help_line("mkdir <name>", "create a folder in RAM");
    help_line("create <name> [text]", "create a file in RAM");
    help_line("cat <name>", "read a file");
    help_line("lnpinfo <file>", "show Nova Picture metadata");
    help_line("edit <name>", "open a one-line editor");
    help_line("rename <old> <new>", "rename a file or folder");
    help_line("rm <name>", "remove a file or empty folder");
    help_line("formatfs", "prepare the NovaFS disk area after confirmation");
    help_line("savefs", "save RAM files to the NovaFS disk area");
    help_line("loadfs", "load RAM files from the NovaFS disk area");
}

static void print_help_lang() {
    vga_set_color(0x0B);
    vga_writeln("NovaC commands");
    help_line("apps", "list official .nc apps in /apps");
    help_line("runNC <file>", "run a .nc file in the current folder");
    help_line("newnc <file>", "create a small NovaC starter file");
    help_line("novac", "show NovaC status");
    help_line("novac help", "show NovaC syntax");
    help_line("novac sample", "run a built-in NovaC sample");
    vga_writeln("Syntax: var name = \"text\"");
    vga_writeln("Syntax: var n = 2 + 3");
    vga_writeln("Syntax: input(name)");
    vga_writeln("Syntax: int << func >>(\"text\")");
    vga_writeln("Syntax: int << func >>(name)");
    vga_writeln("Syntax: << ! >func> if n > 2 { ... }");
    vga_writeln("Syntax: <<While>>! <on> n < 5 { ... }");
    vga_writeln("Try: cd apps  then  runNC calculator.nc");
}

static void print_help_apps() {
    vga_set_color(0x0E);
    vga_writeln("Desktop apps");
    help_line("mouse click", "open desktop icons from the dock");
    help_line("apps", "list official NovaC apps");
    help_line("cd games", "enter NovaC games folder");
    help_line("runNC nova_quest.nc", "play the text adventure");
    help_line("cd apps", "enter the official apps folder");
    help_line("runNC calculator.nc", "open calculator from /apps");
    help_line("runNC settings.nc", "open settings from /apps");
    help_line("runNC documentation.nc", "open documentation from /apps");
    help_line("runNC syntax_test.nc", "test NovaC syntax support");
    help_line("desktop", "return from Terminal to Desktop");
}

static void print_help_input() {
    vga_set_color(0x0C);
    vga_writeln("Input");
    help_line("keyboard", "main input device for now");
    help_line("mouse", "show isolated mouse driver status");
    vga_writeln("PS/2 mouse packets are filtered for safer real PC tests.");
}

static void print_help_power() {
    vga_set_color(0x0C);
    vga_writeln("Power commands");
    help_line("reboot", "restart NovaOS");
    help_line("shutdown", "power off NovaOS when emulator/PC supports it");
}

static void print_help_category(const char* category) {
    if (streq(category, "core")) print_help_core();
    else if (streq(category, "fs")) print_help_fs();
    else if (streq(category, "lang")) print_help_lang();
    else if (streq(category, "apps")) print_help_apps();
    else if (streq(category, "input")) print_help_input();
    else if (streq(category, "power")) print_help_power();
    else {
        vga_writeln("Unknown help category.");
        print_help_index();
    }
}

static void command_ls() {
    int count = ramfs_child_count();
    if (count == 0) {
        vga_writeln("Folder is empty.");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (ramfs_child_is_dir(i)) vga_write("[DIR]  ");
        else vga_write("[FILE] ");
        vga_writeln(ramfs_child_name(i));
    }
}


static void print_tree_from(int parent, int depth) {
    for (int id = 0; id < ramfs_total_slots(); id++) {
        if (!ramfs_entry_used(id) || ramfs_entry_parent(id) != parent || id == parent) continue;
        for (int i = 0; i < depth; i++) vga_write("  ");
        if (ramfs_entry_is_dir(id)) vga_write("[D] ");
        else vga_write("[F] ");
        vga_writeln(ramfs_entry_name(id));
        if (ramfs_entry_is_dir(id)) print_tree_from(id, depth + 1);
    }
}

static void command_tree() {
    vga_set_color(0x0E);
    vga_writeln(ramfs_pwd());
    vga_set_color(0x0F);
    print_tree_from(ramfs_current_id(), 1);
}

static void command_rename(const char* args) {
    args = skip_spaces(args);
    if (!has_arg(args)) {
        vga_writeln("Usage: rename <old> <new>");
        return;
    }

    char old_name[24];
    char new_name[24];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 23) {
        old_name[i] = args[i];
        i++;
    }
    old_name[i] = 0;
    args = skip_spaces(args + i);
    i = 0;
    while (args[i] && args[i] != ' ' && i < 23) {
        new_name[i] = args[i];
        i++;
    }
    new_name[i] = 0;

    if (!has_arg(old_name) || !has_arg(new_name)) {
        vga_writeln("Usage: rename <old> <new>");
        return;
    }

    if (ramfs_rename(old_name, new_name)) vga_writeln("Renamed.");
    else vga_writeln("Cannot rename. Check names or duplicate.");
}

static void command_create(const char* args) {
    args = skip_spaces(args);
    if (!has_arg(args)) {
        vga_writeln("Usage: create <name> [text]");
        return;
    }

    char name[24];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 23) {
        name[i] = args[i];
        i++;
    }
    name[i] = 0;

    const char* content = skip_spaces(args + i);
    if (ramfs_create_file(name, content)) vga_writeln("File created.");
    else vga_writeln("Cannot create file. Check name or duplicate.");
}



static void editor_clear_body(void) {
    for (int y = 5; y < 21; y++) {
        vga_write_at(0, y, "                                                                                ", 0x0F);
    }
}

static void editor_draw_line_number(int y) {
    char n[4];
    int value = y + 1;
    n[0] = (char)('0' + (value / 10));
    n[1] = (char)('0' + (value % 10));
    n[2] = 0;
    vga_write_at(1, 5 + y, n, 0x08);
    vga_write_at(4, 5 + y, "|", 0x08);
}

static void editor_draw(const char* name, char rows[12][71], int cursor_x, int cursor_y) {
    vga_clear(0x0F);
    vga_write_at(2, 1, "NovaEdit", 0x0E);
    vga_write_at(13, 1, name, 0x0B);
    vga_write_at(2, 2, "Arrows move | ENTER new line | BACKSPACE delete | TAB save | ESC cancel", 0x0B);
    vga_write_at(2, 3, "----------------------------------------------------------------------", 0x08);
    editor_clear_body();
    for (int y = 0; y < 12; y++) {
        editor_draw_line_number(y);
        vga_write_at(6, 5 + y, rows[y], 0x0F);
    }
    vga_write_at(6 + cursor_x, 5 + cursor_y, "_", 0x4F);
}

static void editor_import_text(char rows[12][71], const char* text) {
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 71; x++) rows[y][x] = 0;
    }

    int x = 0;
    int y = 0;
    for (int i = 0; text && text[i] && y < 12; i++) {
        char c = text[i];
        if (c == '\n') {
            y++;
            x = 0;
        } else if (c >= 32 && c <= 126 && x < 70) {
            rows[y][x++] = c;
            rows[y][x] = 0;
        }
    }
}

static int row_len(char row[73]) {
    int n = 0;
    while (n < 70 && row[n]) n++;
    return n;
}

static void editor_export_text(char rows[12][71], char* out, int max) {
    int pos = 0;
    int last = 11;
    while (last > 0 && row_len(rows[last]) == 0) last--;

    for (int y = 0; y <= last && pos < max - 1; y++) {
        int len = row_len(rows[y]);
        for (int x = 0; x < len && pos < max - 1; x++) out[pos++] = rows[y][x];
        if (y < last && pos < max - 1) out[pos++] = '\n';
    }
    out[pos] = 0;
}

static void command_edit(const char* args) {
    const char* name = skip_spaces(args);
    if (!has_arg(name)) {
        vga_writeln("Usage: edit <name>");
        return;
    }

    const char* old_text = ramfs_read_file(name);
    if (!old_text) {
        if (!ramfs_create_file(name, "")) {
            vga_writeln("Cannot open file. Check name or folder.");
            return;
        }
        old_text = "";
    }

    char rows[12][71];
    editor_import_text(rows, old_text);
    int cursor_x = row_len(rows[0]);
    int cursor_y = 0;
    editor_draw(name, rows, cursor_x, cursor_y);

    for (;;) {
        char c = keyboard_read_char();
        if (c == 27) {
            terminal_screen();
            vga_writeln("Edit cancelled.");
            prompt();
            return;
        }
        if (c == NOVA_KEY_UP) {
            if (cursor_y > 0) cursor_y--;
            int len = row_len(rows[cursor_y]);
            if (cursor_x > len) cursor_x = len;
        } else if (c == NOVA_KEY_DOWN) {
            if (cursor_y < 11) cursor_y++;
            int len = row_len(rows[cursor_y]);
            if (cursor_x > len) cursor_x = len;
        } else if (c == NOVA_KEY_LEFT) {
            if (cursor_x > 0) cursor_x--;
            else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = row_len(rows[cursor_y]);
            }
        } else if (c == NOVA_KEY_RIGHT) {
            int len = row_len(rows[cursor_y]);
            if (cursor_x < len) cursor_x++;
            else if (cursor_y < 11) {
                cursor_y++;
                cursor_x = 0;
            }
        } else if (c == '\n') {
            if (cursor_y < 11) {
                cursor_y++;
                cursor_x = 0;
            }
        } else if (c == '\b') {
            if (cursor_x > 0) {
                int len = row_len(rows[cursor_y]);
                for (int i = cursor_x - 1; i < len; i++) rows[cursor_y][i] = rows[cursor_y][i + 1];
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = row_len(rows[cursor_y]);
            }
        } else if (c == 0) {
            // ignore
        } else if (c >= 32 && c <= 126) {
            // F2 arrives as '<' on some minimal layouts? No: keep printable text only.
            int len = row_len(rows[cursor_y]);
            if (len < 70) {
                for (int i = len; i >= cursor_x; i--) rows[cursor_y][i + 1] = rows[cursor_y][i];
                rows[cursor_y][cursor_x] = c;
                cursor_x++;
            }
        }

        if (c == NOVA_KEY_LEFT || c == NOVA_KEY_RIGHT || c == NOVA_KEY_UP || c == NOVA_KEY_DOWN ||
            c == '\n' || c == '\b' || (c >= 32 && c <= 126)) {
            editor_draw(name, rows, cursor_x, cursor_y);
        }

        // Ctrl+S is not available yet, so use TAB as save in this small editor.
        if (c == '\t') {
            char buffer[1024];
            editor_export_text(rows, buffer, 1024);
            terminal_screen();
            if (ramfs_write_file(name, buffer)) vga_writeln("File saved.");
            else vga_writeln("Cannot save. This is a protected system file.");
            prompt();
            return;
        }
    }
}


static void command_formatfs() {
    vga_set_color(0x0E);
    vga_writeln("Format NovaFS storage area?");
    vga_writeln("This only uses the NovaFS reserved area, but test in QEMU first.");
    vga_writeln("Press Y to continue or any other key to cancel.");
    vga_set_color(0x0F);
    char c = keyboard_read_char();
    if (c != 'y' && c != 'Y') {
        vga_writeln("Format cancelled.");
        return;
    }
    if (!storage_format_novafs()) {
        vga_writeln("Format failed. Storage not ready or not writable.");
        return;
    }
    if (ramfs_save_to_storage()) vga_writeln("NovaFS formatted and current files saved.");
    else vga_writeln("Format done, but save failed.");
}

static void command_savefs() {
    if (ramfs_save_to_storage()) vga_writeln("NovaFS saved to disk.");
    else vga_writeln("Save failed. Run formatfs first or check storage.");
}

static void command_loadfs() {
    if (ramfs_load_from_storage()) vga_writeln("NovaFS loaded from disk.");
    else vga_writeln("Load failed. No NovaFS image found or storage not ready.");
}

static void command_lnpinfo(const char* args) {
    const char* name = skip_spaces(args);
    if (!has_arg(name)) {
        vga_writeln("Usage: lnpinfo <file.lnp>");
        return;
    }

    const char* data = ramfs_read_file(name);
    if (!data) {
        vga_writeln("LNP file not found.");
        return;
    }

    if (starts_with(data, "LNP1")) {
        vga_writeln("Nova Picture file");
        vga_writeln("Format: LNP1");
        vga_writeln("Current RAMFS image stores placeholder text.");
        vga_writeln("Binary LNP loading comes with initrd/files package.");
    } else {
        vga_writeln("Not an LNP1 file.");
    }
}

static void command_run_nc(const char* args) {
    const char* name = skip_spaces(args);
    if (!has_arg(name)) {
        vga_writeln("Usage: runNC <file.nc>");
        return;
    }

    const char* source = ramfs_read_file(name);
    if (!source) {
        vga_writeln("NovaC file not found.");
        return;
    }

    vga_set_color(0x0E);
    vga_write("[NovaC] Running ");
    vga_writeln(name);
    vga_set_color(0x0F);
    if (novac_run_source(source)) vga_writeln("[NovaC] Done.");
    else vga_writeln("[NovaC] Finished with errors.");
}

static void command_apps() {
    int old_dir = ramfs_current_id();
    ramfs_cd("/");
    if (!ramfs_cd("apps")) {
        ramfs_set_current(old_dir);
        vga_writeln("/apps folder not found.");
        return;
    }

    vga_set_color(0x0E);
    vga_writeln("Official NovaC apps");
    vga_set_color(0x0F);
    int count = ramfs_child_count();
    for (int i = 0; i < count; i++) {
        if (!ramfs_child_is_dir(i)) {
            vga_write("  runNC ");
            vga_writeln(ramfs_child_name(i));
        }
    }
    ramfs_set_current(old_dir);
}

static void command_newnc(const char* args) {
    char filename[32];
    make_novac_filename(filename, args, 32);
    if (!has_arg(filename)) {
        vga_writeln("Usage: newnc <file.nc>");
        return;
    }

    const char* template_code = "var msg = \"My NovaOS NovaC app\"\nint << func >>(msg)";
    if (ramfs_create_file(filename, template_code)) {
        vga_write("Created ");
        vga_writeln(filename);
        vga_writeln("Run it with: runNC <file.nc>");
    } else {
        vga_writeln("Cannot create NovaC file. Check name or duplicate.");
    }
}

static void command_novac(const char* args) {
    args = skip_spaces(args);
    if (!has_arg(args)) {
        vga_writeln("NovaC runtime is inside NovaOS.");
        vga_writeln("Use: apps, runNC <file.nc>, newnc <file>, novac help");
        return;
    }
    if (streq(args, "help")) {
        novac_help();
        return;
    }
    if (streq(args, "sample")) {
        novac_run_source("int << func >>(\"NovaC sample inside NovaOS\")");
        return;
    }
    vga_writeln("Unknown NovaC command. Try: novac help");
}

static void run_command(const char* cmd) {
    vga_putc('\n');

    if (streq(cmd, "again")) {
        if (last_command[0] == 0) {
            vga_writeln("No previous command.");
        } else {
            vga_write("Repeating: ");
            vga_writeln(last_command);
            run_command(last_command);
            return;
        }
    } else if (streq(cmd, "help")) {
        print_help_index();
    } else if (starts_with(cmd, "help ")) {
        print_help_category(skip_spaces(cmd + 5));
    } else if (streq(cmd, "about")) {
        vga_writeln("NovaOS - created by CostaTech.");
        vga_writeln("Built with Assembly, C and C++.");
    } else if (streq(cmd, "storage")) {
        vga_writeln(storage_status_text());
        if (storage_ready()) vga_writeln("Use formatfs, savefs and loadfs for controlled persistence.");
        else vga_writeln("No writable disk bus detected yet.");
    } else if (streq(cmd, "clear")) {
        terminal_screen();
        return;
    } else if (streq(cmd, "desktop") || streq(cmd, "exit")) {
        should_exit = 1;
        return;
    } else if (starts_with(cmd, "echo ")) {
        vga_writeln(cmd + 5);
    } else if (streq(cmd, "pwd")) {
        vga_writeln(ramfs_pwd());
    } else if (streq(cmd, "ls")) {
        command_ls();
    } else if (starts_with(cmd, "cd ")) {
        const char* arg = skip_spaces(cmd + 3);
        if (ramfs_cd(arg)) vga_writeln(ramfs_pwd());
        else vga_writeln("Folder not found.");
    } else if (starts_with(cmd, "mkdir ")) {
        const char* arg = skip_spaces(cmd + 6);
        if (ramfs_mkdir(arg)) vga_writeln("Folder created.");
        else vga_writeln("Cannot create folder. Check name or duplicate.");
    } else if (starts_with(cmd, "create ")) {
        command_create(cmd + 7);
    } else if (starts_with(cmd, "cat ")) {
        const char* arg = skip_spaces(cmd + 4);
        const char* text = ramfs_read_file(arg);
        if (text) vga_writeln(text);
        else vga_writeln("File not found.");
    } else if (starts_with(cmd, "lnpinfo ")) {
        command_lnpinfo(cmd + 8);
    } else if (streq(cmd, "formatfs")) {
        command_formatfs();
    } else if (streq(cmd, "savefs")) {
        command_savefs();
    } else if (streq(cmd, "loadfs")) {
        command_loadfs();
    } else if (starts_with(cmd, "edit ")) {
        command_edit(cmd + 5);
        return;
    } else if (starts_with(cmd, "rename ")) {
        command_rename(cmd + 7);
    } else if (streq(cmd, "tree")) {
        command_tree();
    } else if (starts_with(cmd, "rm ")) {
        const char* arg = skip_spaces(cmd + 3);
        if (ramfs_rm(arg)) vga_writeln("Removed.");
        else vga_writeln("Cannot remove. Missing, protected, or folder not empty.");
    } else if (streq(cmd, "mouse")) {
        mouse_poll();
        if (mouse_enabled()) vga_writeln("Mouse driver: PS/2 polling enabled.");
        else vga_writeln("Mouse driver: compiled but disabled.");
        if (mouse_is_ready()) vga_writeln("Mouse status: ready.");
        else vga_writeln("Mouse status: safe mode / not initialized.");
        vga_write("Position: ");
        vga_write("x=");
        vga_putc((char)('0' + (mouse_x() / 10) % 10));
        vga_putc((char)('0' + mouse_x() % 10));
        vga_write(" y=");
        vga_putc((char)('0' + (mouse_y() / 10) % 10));
        vga_putc((char)('0' + mouse_y() % 10));
        vga_write(" buttons=");
        vga_putc((char)('0' + mouse_buttons()));
        vga_putc('\n');
        vga_writeln("Mouse is filtered to avoid random real-hardware packets.");
    } else if (starts_with(cmd, "runNC ")) {
        command_run_nc(cmd + 6);
    } else if (starts_with(cmd, "runnc ")) {
        command_run_nc(cmd + 6);
    } else if (streq(cmd, "apps")) {
        command_apps();
    } else if (starts_with(cmd, "newnc ")) {
        command_newnc(cmd + 6);
    } else if (streq(cmd, "novac") || starts_with(cmd, "novac ")) {
        command_novac(cmd + 6);
    } else if (streq(cmd, "reboot")) {
        vga_writeln("Rebooting NovaOS...");
        system_reboot();
    } else if (streq(cmd, "shutdown")) {
        vga_writeln("Shutting down NovaOS...");
        system_shutdown();
    } else if (cmd[0] == 0) {
        // empty command
    } else {
        vga_writeln("Unknown command. Type help.");
    }

    prompt();
}

void shell_run() {
    terminal_screen();
    line_len = 0;
    should_exit = 0;
    last_command[0] = 0;
    for (;;) {
        if (should_exit) return;
        char c = keyboard_read_char();
        if (c == '\n') {
            line[line_len] = 0;
            if (line[0] != 0 && !streq(line, "again")) copy_text(last_command, line, 128);
            run_command(line);
            line_len = 0;
        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                vga_putc('\b');
            }
        } else if (c >= 32 && c <= 126) {
            if (line_len < 127) {
                line[line_len++] = c;
                vga_putc(c);
            }
        }
    }
}
