#include "nova.h"

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static const char* skip_spaces(const char* text) {
    while (*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') text++;
    return text;
}

static int text_eq_len(const char* a, const char* b, int len) {
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) return 0;
    }
    return a[len] == 0 && b[len] == 0;
}

static int copy_name(char* dst, const char* src, int max) {
    int i = 0;
    src = skip_spaces(src);
    while (((src[i] >= 'a' && src[i] <= 'z') || (src[i] >= 'A' && src[i] <= 'Z') ||
            (src[i] >= '0' && src[i] <= '9') || src[i] == '_') && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
    return i;
}

static void copy_value(char* dst, const char* src, int max) {
    int i = 0;
    src = skip_spaces(src);
    while (src[i] && src[i] != '\n' && src[i] != '\r' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t')) i--;
    dst[i] = 0;
}

static int print_quoted(const char* text) {
    while (*text && *text != '"') text++;
    if (*text != '"') return 0;
    text++;
    while (*text && *text != '"') {
        vga_putc(*text);
        text++;
    }
    if (*text != '"') return 0;
    vga_putc('\n');
    return 1;
}

struct TencleVar {
    char name[24];
    char value[48];
};

static TencleVar vars[8];
static int var_count = 0;

static void clear_vars() {
    var_count = 0;
    for (int i = 0; i < 8; i++) {
        vars[i].name[0] = 0;
        vars[i].value[0] = 0;
    }
}

static const char* find_var(const char* name, int len) {
    for (int i = 0; i < var_count; i++) {
        if (text_eq_len(vars[i].name, name, len)) return vars[i].value;
    }
    return 0;
}

static int run_var_line(const char* line) {
    if (!starts_with(line, "var ")) return 0;
    if (var_count >= 8) {
        vga_writeln("[TencleLang] Too many variables.");
        return -1;
    }

    line += 4;
    int name_len = copy_name(vars[var_count].name, line, 24);
    if (name_len <= 0) {
        vga_writeln("[TencleLang] Variable needs a name.");
        return -1;
    }
    line = skip_spaces(line + name_len);
    if (*line != '=') {
        vga_writeln("[TencleLang] Variable needs =");
        return -1;
    }

    copy_value(vars[var_count].value, line + 1, 48);
    var_count++;
    return 1;
}

static int print_value(const char* text) {
    text = skip_spaces(text);
    if (*text != '(') return 0;
    text = skip_spaces(text + 1);

    if (*text == '"') return print_quoted(text);

    char name[24];
    int name_len = copy_name(name, text, 24);
    if (name_len <= 0) return 0;

    const char* value = find_var(name, name_len);
    if (!value) {
        vga_writeln("[TencleLang] Unknown variable.");
        return -1;
    }

    vga_writeln(value);
    return 1;
}

void tenclelang_init(void) {
    // Runtime is ready. Real compiler comes later.
}

void tenclelang_help(void) {
    vga_set_color(0x0E);
    vga_writeln("TencleLang inside NovaOS");
    vga_set_color(0x0F);
    vga_writeln("Official syntax now:");
    vga_writeln("  var name = value");
    vga_writeln("  int << func >>(\"text\")");
    vga_writeln("  int << func >>(name)");
    vga_writeln("Normal print/if/while syntax is not accepted.");
}

int tenclelang_run_source(const char* source) {
    source = skip_spaces(source);
    if (!source || !source[0]) {
        vga_writeln("[TencleLang] Empty source.");
        return 0;
    }

    clear_vars();

    const char* line = source;
    while (*line) {
        line = skip_spaces(line);
        if (!*line) break;

        int var_result = run_var_line(line);
        if (var_result < 0) return 0;
        if (var_result == 0) {
            if (!starts_with(line, "int << func >>")) {
                vga_writeln("[TencleLang] Syntax error.");
                vga_writeln("Use: var name = value");
                vga_writeln("Or:  int << func >>(name)");
                return 0;
            }

            int printed = print_value(line + 14);
            if (printed <= 0) {
                vga_writeln("[TencleLang] Cannot print this value.");
                return 0;
            }
        }

        while (*line && *line != '\n' && *line != '\r') line++;
    }

    return 1;
}
