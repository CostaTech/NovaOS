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

void tenclelang_init(void) {
    // Runtime is ready. Real compiler comes later.
}

void tenclelang_help(void) {
    vga_set_color(0x0E);
    vga_writeln("TencleLang inside NovaOS");
    vga_set_color(0x0F);
    vga_writeln("Official syntax now:");
    vga_writeln("  int << func >>(\"text\")");
    vga_writeln("Only this print form is accepted for now.");
}

int tenclelang_run_source(const char* source) {
    source = skip_spaces(source);
    if (!source || !source[0]) {
        vga_writeln("[TencleLang] Empty source.");
        return 0;
    }

    if (!starts_with(source, "int << func >>")) {
        vga_writeln("[TencleLang] Syntax error.");
        vga_writeln("Use: int << func >>(\"text\")");
        return 0;
    }

    if (!print_quoted(source)) {
        vga_writeln("[TencleLang] Missing quoted text.");
        return 0;
    }

    return 1;
}
