#include "nova.h"

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
    while (*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') text++;
    return text;
}

static int parse_number(const char* text, int* value) {
    int n = 0;
    int seen = 0;
    while (*text >= '0' && *text <= '9') {
        n = n * 10 + (*text - '0');
        text++;
        seen = 1;
    }
    if (!seen) return 0;
    *value = n;
    return 1;
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

static int run_statement(const char* stmt) {
    stmt = skip_spaces(stmt);
    if (*stmt == 0) return 1;

    if (starts_with(stmt, "print ")) {
        return print_quoted(stmt + 6);
    }

    if (starts_with(stmt, "int << func >>")) {
        return print_quoted(stmt);
    }

    if (starts_with(stmt, "color ")) {
        int color = 0;
        if (!parse_number(skip_spaces(stmt + 6), &color)) return 0;
        vga_set_color((u8)(color & 0xFF));
        return 1;
    }

    if (streq(stmt, "clear")) {
        vga_clear(0x0F);
        return 1;
    }

    vga_write("[TencleLang] Unknown statement: ");
    vga_writeln(stmt);
    return 0;
}

void tenclelang_init(void) {
    // Runtime is ready. Real compiler comes later.
}

void tenclelang_help(void) {
    vga_set_color(0x0E);
    vga_writeln("TencleLang inside NovaOS");
    vga_set_color(0x0F);
    vga_writeln("Supported now:");
    vga_writeln("  print \"text\"");
    vga_writeln("  int << func >>(\"text\")");
    vga_writeln("  color <vga_color>");
    vga_writeln("  clear");
    vga_writeln("Separate statements with ; inside one file line.");
}

int tenclelang_run_source(const char* source) {
    if (!source || !source[0]) {
        vga_writeln("[TencleLang] Empty source.");
        return 0;
    }

    char stmt[160];
    int pos = 0;
    int ok = 1;

    for (int i = 0;; i++) {
        char c = source[i];
        if (c == ';' || c == 0) {
            stmt[pos] = 0;
            if (!run_statement(stmt)) ok = 0;
            pos = 0;
            if (c == 0) break;
        } else if (pos < 159) {
            stmt[pos++] = c;
        }
    }

    vga_set_color(0x0F);
    return ok;
}
