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
    help_line("help lang", "TencleLang commands");
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
    help_line("edit <name>", "open a one-line editor");
    help_line("rename <old> <new>", "rename a file or folder");
    help_line("rm <name>", "remove a file or empty folder");
}

static void print_help_lang() {
    vga_set_color(0x0B);
    vga_writeln("TencleLang commands");
    help_line("tencle", "show TencleLang status");
    help_line("tencle help", "show TencleLang syntax");
    help_line("tencle demo", "run a built-in demo");
    help_line("tlrun <file>", "run a .tlang file in current folder");
    vga_writeln("Syntax: int << func >>(\"text\")");
    vga_writeln("Try: cd apps  then  tlrun hello.tlang");
}

static void print_help_apps() {
    vga_set_color(0x0E);
    vga_writeln("Desktop apps");
    help_line("F", "Files app");
    help_line("T", "Terminal app");
    help_line("S", "Settings app");
    help_line("A", "About app");
    help_line("G", "Galaxy demo");
}

static void print_help_input() {
    vga_set_color(0x0C);
    vga_writeln("Input");
    help_line("keyboard", "main input device for now");
    help_line("mouse", "show isolated mouse driver status");
    vga_writeln("Real PS/2 mouse packets stay disabled until stable.");
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


static int text_len_local(const char* text) {
    int n = 0;
    while (text && text[n]) n++;
    return n;
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

    char buffer[192];
    copy_text(buffer, old_text, 192);
    int len = text_len_local(buffer);

    vga_clear(0x0F);
    vga_write_at(2, 1, "NovaEdit", 0x0E);
    vga_write_at(2, 2, "ENTER saves. ESC cancels. BACKSPACE deletes.", 0x0B);
    vga_write_at(2, 4, "File:", 0x0A);
    vga_write_at(8, 4, name, 0x0F);
    vga_write_at(2, 6, "Text:", 0x0A);
    vga_set_color(0x0F);
    vga_write_at(2, 8, buffer, 0x0F);

    for (;;) {
        char c = keyboard_read_char();
        if (c == 27) {
            terminal_screen();
            vga_writeln("Edit cancelled.");
            prompt();
            return;
        }
        if (c == '\n') {
            ramfs_write_file(name, buffer);
            terminal_screen();
            vga_writeln("File saved.");
            prompt();
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                buffer[len] = 0;
                vga_putc('\b');
            }
        } else if (c >= 32 && c <= 126) {
            if (len < 191) {
                buffer[len++] = c;
                buffer[len] = 0;
                vga_putc(c);
            }
        }
    }
}

static void command_tlrun(const char* args) {
    const char* name = skip_spaces(args);
    if (!has_arg(name)) {
        vga_writeln("Usage: tlrun <file.tlang>");
        return;
    }

    const char* source = ramfs_read_file(name);
    if (!source) {
        vga_writeln("TencleLang file not found.");
        return;
    }

    vga_set_color(0x0E);
    vga_write("[TencleLang] Running ");
    vga_writeln(name);
    vga_set_color(0x0F);
    if (tenclelang_run_source(source)) vga_writeln("[TencleLang] Done.");
    else vga_writeln("[TencleLang] Finished with errors.");
}

static void command_tencle(const char* args) {
    args = skip_spaces(args);
    if (!has_arg(args)) {
        vga_writeln("TencleLang runtime is inside NovaOS.");
        vga_writeln("Use: tencle help, tencle demo, tlrun <file.tlang>");
        return;
    }
    if (streq(args, "help")) {
        tenclelang_help();
        return;
    }
    if (streq(args, "demo")) {
        tenclelang_run_source("int << func >>(\"TencleLang demo inside NovaOS\")");
        return;
    }
    vga_writeln("Unknown TencleLang command. Try: tencle help");
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
        else vga_writeln("Cannot remove. File missing or folder not empty.");
    } else if (streq(cmd, "mouse")) {
        vga_writeln("Mouse driver: isolated safe mode.");
        vga_writeln("Real PS/2 packets are not enabled yet.");
        vga_write("Position placeholder: ");
        vga_write("x=");
        vga_putc((char)('0' + (mouse_x() / 10) % 10));
        vga_putc((char)('0' + mouse_x() % 10));
        vga_write(" y=");
        vga_putc((char)('0' + (mouse_y() / 10) % 10));
        vga_putc((char)('0' + mouse_y() % 10));
        vga_putc('\n');
    } else if (starts_with(cmd, "tlrun ")) {
        command_tlrun(cmd + 6);
    } else if (streq(cmd, "tencle") || starts_with(cmd, "tencle ")) {
        command_tencle(cmd + 6);
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
