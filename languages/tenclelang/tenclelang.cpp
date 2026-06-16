#include "nova.h"

struct TencleValue {
    int is_string;
    int number;
    char text[48];
};

struct TencleVar {
    char name[24];
    TencleValue value;
};

static TencleVar vars[16];
static int var_count = 0;

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static int str_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static const char* skip_spaces(const char* text) {
    while (*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') text++;
    return text;
}

static int text_len(const char* text) {
    int n = 0;
    while (text && text[n]) n++;
    return n;
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

static void clear_vars() {
    var_count = 0;
    for (int i = 0; i < 16; i++) {
        vars[i].name[0] = 0;
        vars[i].value.is_string = 0;
        vars[i].value.number = 0;
        vars[i].value.text[0] = 0;
    }
}

static int find_var_id(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (str_eq(vars[i].name, name)) return i;
    }
    return -1;
}

static TencleValue make_number(int n) {
    TencleValue v;
    v.is_string = 0;
    v.number = n;
    v.text[0] = 0;
    return v;
}

static TencleValue make_string(const char* text) {
    TencleValue v;
    v.is_string = 1;
    v.number = 0;
    copy_text(v.text, text, 48);
    return v;
}

static void print_number(int n) {
    char out[16];
    int pos = 0;
    if (n == 0) {
        vga_putc('0');
        return;
    }
    if (n < 0) {
        vga_putc('-');
        n = -n;
    }
    while (n > 0 && pos < 15) {
        out[pos++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (pos > 0) vga_putc(out[--pos]);
}

static int parse_int(const char** p, int* ok) {
    const char* s = skip_spaces(*p);
    int sign = 1;
    int value = 0;
    *ok = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        *ok = 1;
        value = value * 10 + (*s - '0');
        s++;
    }
    *p = s;
    return value * sign;
}

static int text_to_int(const char* text, int* ok) {
    const char* p = text;
    int n = parse_int(&p, ok);
    p = skip_spaces(p);
    if (*p != 0) *ok = 0;
    return n;
}

static int eval_number(const char* expr, int* ok) {
    const char* p = skip_spaces(expr);
    int left_ok = 0;
    int left = 0;
    char name[24];
    int name_len = copy_name(name, p, 24);

    if (name_len > 0 && !(name[0] >= '0' && name[0] <= '9')) {
        int id = find_var_id(name);
        if (id < 0 || vars[id].value.is_string) {
            *ok = 0;
            return 0;
        }
        left = vars[id].value.number;
        p += name_len;
        left_ok = 1;
    } else {
        left = parse_int(&p, &left_ok);
    }

    if (!left_ok) {
        *ok = 0;
        return 0;
    }

    p = skip_spaces(p);
    if (*p == 0 || *p == '\n' || *p == '\r' || *p == ')' || *p == '{') {
        *ok = 1;
        return left;
    }

    char op = *p++;
    int right_ok = 0;
    int right = 0;
    p = skip_spaces(p);
    name_len = copy_name(name, p, 24);
    if (name_len > 0 && !(name[0] >= '0' && name[0] <= '9')) {
        int id = find_var_id(name);
        if (id < 0 || vars[id].value.is_string) {
            *ok = 0;
            return 0;
        }
        right = vars[id].value.number;
        right_ok = 1;
    } else {
        right = parse_int(&p, &right_ok);
    }

    if (!right_ok) {
        *ok = 0;
        return 0;
    }

    *ok = 1;
    if (op == '+') return left + right;
    if (op == '-') return left - right;
    if (op == '*') return left * right;
    if (op == '/') return right == 0 ? 0 : left / right;
    *ok = 0;
    return 0;
}

static TencleValue eval_value(const char* expr, int* ok) {
    expr = skip_spaces(expr);
    if (*expr == '"') {
        char tmp[48];
        int i = 0;
        expr++;
        while (expr[i] && expr[i] != '"' && i < 47) {
            tmp[i] = expr[i];
            i++;
        }
        tmp[i] = 0;
        *ok = 1;
        return make_string(tmp);
    }

    char name[24];
    int name_len = copy_name(name, expr, 24);
    const char* after_name = skip_spaces(expr + name_len);
    if (name_len > 0 && (*after_name == 0 || *after_name == '\n' || *after_name == '\r' || *after_name == ')')) {
        int id = find_var_id(name);
        if (id >= 0) {
            *ok = 1;
            return vars[id].value;
        }
    }

    int number_ok = 0;
    int number = eval_number(expr, &number_ok);
    *ok = number_ok;
    return make_number(number);
}

static int set_var(const char* name, TencleValue value) {
    int id = find_var_id(name);
    if (id < 0) {
        if (var_count >= 16) return 0;
        id = var_count++;
        copy_text(vars[id].name, name, 24);
    }
    vars[id].value = value;
    return 1;
}

static int eval_condition(const char* cond) {
    char left_expr[32];
    char right_expr[32];
    int i = 0;
    cond = skip_spaces(cond);
    while (cond[i] && cond[i] != '>' && cond[i] != '<' && cond[i] != '=' && i < 31) {
        left_expr[i] = cond[i];
        i++;
    }
    left_expr[i] = 0;

    char op = cond[i];
    if (!op) return 0;
    int double_eq = op == '=' && cond[i + 1] == '=';
    i += double_eq ? 2 : 1;

    int j = 0;
    while (cond[i] && cond[i] != '{' && j < 31) right_expr[j++] = cond[i++];
    right_expr[j] = 0;

    int value_left_ok = 0;
    int value_right_ok = 0;
    TencleValue left_value = eval_value(left_expr, &value_left_ok);
    TencleValue right_value = eval_value(right_expr, &value_right_ok);

    if (double_eq && value_left_ok && value_right_ok && left_value.is_string && right_value.is_string) {
        return str_eq(left_value.text, right_value.text);
    }

    int ok_left = 0;
    int ok_right = 0;
    int left = eval_number(left_expr, &ok_left);
    int right = eval_number(right_expr, &ok_right);
    if (!ok_left || !ok_right) return 0;

    if (op == '>') return left > right;
    if (op == '<') return left < right;
    if (op == '=' && double_eq) return left == right;
    return 0;
}

static int split_lines(const char* source, char lines[40][96]) {
    int line = 0;
    int col = 0;
    for (int i = 0; source[i] && line < 40; i++) {
        if (source[i] == '\r') continue;
        if (source[i] == '\n') {
            lines[line][col] = 0;
            line++;
            col = 0;
        } else if (col < 95) {
            lines[line][col++] = source[i];
        }
    }
    if (line < 40 && col > 0) {
        lines[line][col] = 0;
        line++;
    }
    return line;
}

static int find_block_end(char lines[40][96], int start, int end) {
    int depth = 0;
    for (int i = start; i < end; i++) {
        const char* line = lines[i];
        for (int j = 0; line[j]; j++) {
            if (line[j] == '{') depth++;
            if (line[j] == '}') {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return -1;
}

static int run_lines(char lines[40][96], int start, int end);

static int run_print(const char* line) {
    const char* p = skip_spaces(line + 14);
    if (*p != '(') return 0;
    p++;
    int len = text_len(p);
    char expr[64];
    int out = 0;
    for (int i = 0; i < len && p[i] && p[i] != ')' && out < 63; i++) expr[out++] = p[i];
    expr[out] = 0;

    int ok = 0;
    TencleValue value = eval_value(expr, &ok);
    if (!ok) return 0;
    if (value.is_string) vga_writeln(value.text);
    else {
        print_number(value.number);
        vga_putc('\n');
    }
    return 1;
}

static int run_var(const char* line) {
    line = skip_spaces(line + 4);
    char name[24];
    int name_len = copy_name(name, line, 24);
    if (name_len <= 0) return 0;
    line = skip_spaces(line + name_len);
    if (*line != '=') return 0;
    int ok = 0;
    TencleValue value = eval_value(line + 1, &ok);
    if (!ok) return 0;
    return set_var(name, value);
}

static int run_input(const char* line) {
    const char* p = skip_spaces(line + 5);
    if (*p != '(') return 0;
    p++;

    char name[24];
    int name_len = copy_name(name, p, 24);
    if (name_len <= 0) return 0;

    char buffer[48];
    int len = 0;
    buffer[0] = 0;
    vga_set_color(0x0B);
    vga_write("? ");
    vga_set_color(0x0F);

    for (;;) {
        char c = keyboard_read_char();
        if (c == '\n') {
            vga_putc('\n');
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                buffer[len] = 0;
                vga_putc('\b');
            }
        } else if (c >= 32 && c <= 126 && len < 47) {
            buffer[len++] = c;
            buffer[len] = 0;
            vga_putc(c);
        }
    }

    int ok_number = 0;
    int n = text_to_int(buffer, &ok_number);
    if (ok_number) return set_var(name, make_number(n));
    return set_var(name, make_string(buffer));
}

static int run_if(char lines[40][96], int* index, int end) {
    int block_end = find_block_end(lines, *index, end);
    if (block_end < 0) return 0;

    const char* cond = skip_spaces(lines[*index] + text_len("<< ! >func> if"));
    int true_branch = eval_condition(cond);
    if (true_branch) {
        if (!run_lines(lines, *index + 1, block_end)) return 0;
        if (block_end + 1 < end && starts_with(skip_spaces(lines[block_end + 1]), ">> func << else")) {
            int else_end = find_block_end(lines, block_end + 1, end);
            if (else_end < 0) return 0;
            *index = else_end;
            return 1;
        }
    } else if (block_end + 1 < end && starts_with(skip_spaces(lines[block_end + 1]), ">> func << else")) {
        int else_end = find_block_end(lines, block_end + 1, end);
        if (else_end < 0) return 0;
        if (!run_lines(lines, block_end + 2, else_end)) return 0;
        *index = else_end;
        return 1;
    }

    *index = block_end;
    return 1;
}

static int run_while(char lines[40][96], int* index, int end) {
    int block_end = find_block_end(lines, *index, end);
    if (block_end < 0) return 0;

    const char* cond = skip_spaces(lines[*index] + text_len("<<While>>! <on>"));
    int guard = 0;
    while (eval_condition(cond)) {
        if (!run_lines(lines, *index + 1, block_end)) return 0;
        guard++;
        if (guard > 64) {
            vga_writeln("[TencleLang] While stopped: too many loops.");
            return 0;
        }
    }

    *index = block_end;
    return 1;
}

static int run_lines(char lines[40][96], int start, int end) {
    for (int i = start; i < end; i++) {
        const char* line = skip_spaces(lines[i]);
        if (!line[0] || str_eq(line, "}") || starts_with(line, "//")) continue;
        if (starts_with(line, "var ")) {
            if (!run_var(line)) return 0;
        } else if (starts_with(line, "input")) {
            if (!run_input(line)) return 0;
        } else if (starts_with(line, "int << func >>")) {
            if (!run_print(line)) return 0;
        } else if (starts_with(line, "<< ! >func> if")) {
            if (!run_if(lines, &i, end)) return 0;
        } else if (starts_with(line, "<<While>>! <on>")) {
            if (!run_while(lines, &i, end)) return 0;
        } else if (starts_with(line, ">> func << else")) {
            continue;
        } else {
            vga_write("[TencleLang] Unknown line: ");
            vga_writeln(line);
            return 0;
        }
    }
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
    vga_writeln("  var name = \"text\"");
    vga_writeln("  var n = 2 + 3");
    vga_writeln("  input(name)");
    vga_writeln("  int << func >>(name)");
    vga_writeln("  << ! >func> if n > 2 { ... } >> func << else { ... }");
    vga_writeln("  <<While>>! <on> n < 5 { ... }");
}

int tenclelang_run_source(const char* source) {
    source = skip_spaces(source);
    if (!source || !source[0]) {
        vga_writeln("[TencleLang] Empty source.");
        return 0;
    }

    clear_vars();
    char lines[40][96];
    int count = split_lines(source, lines);
    return run_lines(lines, 0, count);
}
