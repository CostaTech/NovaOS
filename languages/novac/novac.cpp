#include "nova.h"

#define NOVAC_MAX_LINES 96
#define NOVAC_LINE_LEN 128

struct NovaCValue {
    int is_string;
    int number;
    char text[48];
};

struct NovaCVar {
    char name[24];
    NovaCValue value;
};

static NovaCVar vars[16];
static int var_count = 0;
static int novac_error_reported = 0;
static void print_number(int n);

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


static void print_small_number(int n) {
    print_number(n);
}

static void novac_report_error(int line_no, const char* message, const char* line) {
    if (novac_error_reported) return;
    novac_error_reported = 1;
    vga_set_color(0x0C);
    vga_write("[NovaC] Error at line ");
    print_small_number(line_no);
    vga_write(": ");
    vga_writeln(message);
    if (line && line[0]) {
        vga_set_color(0x08);
        vga_write("  > ");
        vga_writeln(line);
    }
    vga_set_color(0x0F);
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

static NovaCValue make_number(int n) {
    NovaCValue v;
    v.is_string = 0;
    v.number = n;
    v.text[0] = 0;
    return v;
}

static NovaCValue make_string(const char* text) {
    NovaCValue v;
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
    if (op == '%') return right == 0 ? 0 : left % right;
    *ok = 0;
    return 0;
}

static NovaCValue eval_value(const char* expr, int* ok) {
    expr = skip_spaces(expr);
    if (starts_with(expr, "on")) {
        const char* after = skip_spaces(expr + 2);
        if (*after == 0 || *after == ')' || *after == '}' || *after == '{') {
            *ok = 1;
            return make_number(1);
        }
    }
    if (starts_with(expr, "off")) {
        const char* after = skip_spaces(expr + 3);
        if (*after == 0 || *after == ')' || *after == '}' || *after == '{') {
            *ok = 1;
            return make_number(0);
        }
    }
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

static int set_var(const char* name, NovaCValue value) {
    int id = find_var_id(name);
    if (id < 0) {
        if (var_count >= 16) return 0;
        id = var_count++;
        copy_text(vars[id].name, name, 24);
    }
    vars[id].value = value;
    return 1;
}

static void copy_range_trim(char* dst, const char* src, int start, int end, int max) {
    while (start < end && (src[start] == ' ' || src[start] == '\t')) start++;
    while (end > start && (src[end - 1] == ' ' || src[end - 1] == '\t' || src[end - 1] == '{')) end--;
    int out = 0;
    for (int i = start; i < end && out < max - 1; i++) dst[out++] = src[i];
    dst[out] = 0;
}

static int find_logic_op(const char* cond, const char* op) {
    int op_len = text_len(op);
    int in_string = 0;
    for (int i = 0; cond[i]; i++) {
        if (cond[i] == '"') in_string = !in_string;
        if (in_string) continue;
        int ok = 1;
        for (int j = 0; j < op_len; j++) {
            if (cond[i + j] != op[j]) {
                ok = 0;
                break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

static int eval_condition(const char* cond);

static int eval_simple_condition(const char* cond) {
    char left_expr[48];
    char right_expr[48];
    int op_pos = -1;
    int op_len = 0;
    char op1 = 0;
    char op2 = 0;

    cond = skip_spaces(cond);
    for (int i = 0; cond[i] && cond[i] != '{'; i++) {
        if (cond[i] == '>' || cond[i] == '<' || cond[i] == '=' || cond[i] == '!') {
            op_pos = i;
            op1 = cond[i];
            if (cond[i + 1] == '=') {
                op2 = '=';
                op_len = 2;
            } else {
                op_len = 1;
            }
            break;
        }
    }

    if (op_pos < 0) {
        int ok = 0;
        NovaCValue value = eval_value(cond, &ok);
        if (!ok) return 0;
        if (value.is_string) return value.text[0] != 0;
        return value.number != 0;
    }

    int cond_len = text_len(cond);
    copy_range_trim(left_expr, cond, 0, op_pos, 48);
    copy_range_trim(right_expr, cond, op_pos + op_len, cond_len, 48);

    int value_left_ok = 0;
    int value_right_ok = 0;
    NovaCValue left_value = eval_value(left_expr, &value_left_ok);
    NovaCValue right_value = eval_value(right_expr, &value_right_ok);

    if (value_left_ok && value_right_ok && left_value.is_string && right_value.is_string) {
        if (op1 == '=' && op2 == '=') return str_eq(left_value.text, right_value.text);
        if (op1 == '!' && op2 == '=') return !str_eq(left_value.text, right_value.text);
        return 0;
    }

    int ok_left = 0;
    int ok_right = 0;
    int left = eval_number(left_expr, &ok_left);
    int right = eval_number(right_expr, &ok_right);
    if (!ok_left || !ok_right) return 0;

    if (op1 == '>' && op2 == '=') return left >= right;
    if (op1 == '<' && op2 == '=') return left <= right;
    if (op1 == '!' && op2 == '=') return left != right;
    if (op1 == '=' && op2 == '=') return left == right;
    if (op1 == '>') return left > right;
    if (op1 == '<') return left < right;
    return 0;
}

static int eval_condition(const char* cond) {
    cond = skip_spaces(cond);
    if (starts_with(cond, "not ")) return !eval_condition(cond + 4);

    int or_pos = find_logic_op(cond, " or ");
    if (or_pos >= 0) {
        char left[64];
        char right[64];
        copy_range_trim(left, cond, 0, or_pos, 64);
        copy_range_trim(right, cond, or_pos + 4, text_len(cond), 64);
        return eval_condition(left) || eval_condition(right);
    }

    int and_pos = find_logic_op(cond, " and ");
    if (and_pos >= 0) {
        char left[64];
        char right[64];
        copy_range_trim(left, cond, 0, and_pos, 64);
        copy_range_trim(right, cond, and_pos + 5, text_len(cond), 64);
        return eval_condition(left) && eval_condition(right);
    }

    return eval_simple_condition(cond);
}

static int split_lines(const char* source, char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN]) {
    int line = 0;
    int col = 0;
    for (int i = 0; source[i] && line < NOVAC_MAX_LINES; i++) {
        if (source[i] == '\r') continue;
        if (source[i] == '\n') {
            lines[line][col] = 0;
            line++;
            col = 0;
        } else if (source[i] == '}' && col < NOVAC_LINE_LEN - 1) {
            lines[line][col++] = source[i];
            lines[line][col] = 0;
            line++;
            col = 0;
            while (source[i + 1] == ' ' || source[i + 1] == '\t') i++;
        } else if (col < NOVAC_LINE_LEN - 1) {
            lines[line][col++] = source[i];
        }
    }
    if (line < NOVAC_MAX_LINES && col > 0) {
        lines[line][col] = 0;
        line++;
    }
    return line;
}

static int find_block_end(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int start, int end) {
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

static int run_lines(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int start, int end);

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
    NovaCValue value = eval_value(expr, &ok);
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
    NovaCValue value = eval_value(line + 1, &ok);
    if (!ok) return 0;
    return set_var(name, value);
}

static int run_assign(const char* line) {
    char name[24];
    int name_len = copy_name(name, line, 24);
    if (name_len <= 0) return 0;
    line = skip_spaces(line + name_len);
    if (*line != '=' || line[1] == '=') return 0;
    int ok = 0;
    NovaCValue value = eval_value(line + 1, &ok);
    if (!ok) return 0;
    return set_var(name, value);
}

static int run_theme(const char* line) {
    const char* p = skip_spaces(line + 5);
    if (*p != '(') return 0;
    p++;
    char expr[64];
    int out = 0;
    while (*p && *p != ')' && out < 63) expr[out++] = *p++;
    expr[out] = 0;

    int ok = 0;
    int theme = eval_number(expr, &ok);
    if (!ok) return 0;
    desktop_set_theme(theme);
    vga_write("[NovaC] Theme changed to ");
    vga_writeln(desktop_theme_name());
    return 1;
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

static int skip_if_chain(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int pos, int end) {
    while (pos < end) {
        const char* line = skip_spaces(lines[pos]);
        if (starts_with(line, "elif ") || starts_with(line, ">> func << else")) {
            int branch_end = find_block_end(lines, pos, end);
            if (branch_end < 0) return pos;
            pos = branch_end + 1;
        } else {
            break;
        }
    }
    return pos - 1;
}

static int run_if(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int* index, int end) {
    int block_end = find_block_end(lines, *index, end);
    if (block_end < 0) return 0;

    const char* cond = skip_spaces(lines[*index] + text_len("<< ! >func> if"));
    if (eval_condition(cond)) {
        if (!run_lines(lines, *index + 1, block_end)) return 0;
        *index = skip_if_chain(lines, block_end + 1, end);
        return 1;
    }

    int pos = block_end + 1;
    while (pos < end && starts_with(skip_spaces(lines[pos]), "elif ")) {
        int elif_end = find_block_end(lines, pos, end);
        if (elif_end < 0) return 0;
        const char* elif_cond = skip_spaces(lines[pos] + text_len("elif"));
        if (eval_condition(elif_cond)) {
            if (!run_lines(lines, pos + 1, elif_end)) return 0;
            *index = skip_if_chain(lines, elif_end + 1, end);
            return 1;
        }
        pos = elif_end + 1;
    }

    if (pos < end && starts_with(skip_spaces(lines[pos]), ">> func << else")) {
        int else_end = find_block_end(lines, pos, end);
        if (else_end < 0) return 0;
        if (!run_lines(lines, pos + 1, else_end)) return 0;
        *index = else_end;
        return 1;
    }

    *index = block_end;
    return 1;
}

static int run_for(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int* index, int end) {
    int block_end = find_block_end(lines, *index, end);
    if (block_end < 0) return 0;

    const char* p = skip_spaces(lines[*index] + text_len("int < for >"));
    char name[24];
    int name_len = copy_name(name, p, 24);
    if (name_len <= 0) return 0;
    p = skip_spaces(p + name_len);
    if (!starts_with(p, "in ")) return 0;
    p = skip_spaces(p + 3);

    int ok = 0;
    int count = eval_number(p, &ok);
    if (!ok || count < 0) return 0;
    if (count > 128) count = 128;

    for (int i = 0; i < count; i++) {
        set_var(name, make_number(i));
        if (!run_lines(lines, *index + 1, block_end)) return 0;
    }

    *index = block_end;
    return 1;
}

static int run_while(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int* index, int end) {
    int block_end = find_block_end(lines, *index, end);
    if (block_end < 0) return 0;

    const char* cond = skip_spaces(lines[*index] + text_len("<<While>>! <on>"));
    int guard = 0;
    while (eval_condition(cond)) {
        if (!run_lines(lines, *index + 1, block_end)) return 0;
        guard++;
        if (guard > 64) {
            vga_writeln("[NovaC] While stopped: too many loops.");
            return 0;
        }
    }

    *index = block_end;
    return 1;
}

static int run_lines(char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN], int start, int end) {
    for (int i = start; i < end; i++) {
        const char* line = skip_spaces(lines[i]);
        if (!line[0] || str_eq(line, "}") || starts_with(line, "//") || starts_with(line, "#")) continue;
        if (starts_with(line, "var ")) {
            if (!run_var(line)) {
                novac_report_error(i + 1, "invalid variable declaration", line);
                return 0;
            }
        } else if (starts_with(line, "input")) {
            if (!run_input(line)) {
                novac_report_error(i + 1, "invalid input syntax", line);
                return 0;
            }
        } else if (starts_with(line, "int << func >>")) {
            if (!run_print(line)) {
                novac_report_error(i + 1, "invalid print syntax or expression", line);
                return 0;
            }
        } else if (starts_with(line, "theme")) {
            if (!run_theme(line)) {
                novac_report_error(i + 1, "invalid theme syntax, use theme(0..3)", line);
                return 0;
            }
        } else if (starts_with(line, "<< ! >func> if")) {
            if (!run_if(lines, &i, end)) {
                novac_report_error(i + 1, "invalid if/elif/else block", line);
                return 0;
            }
        } else if (starts_with(line, "<<While>>! <on>")) {
            if (!run_while(lines, &i, end)) {
                novac_report_error(i + 1, "invalid while block", line);
                return 0;
            }
        } else if (starts_with(line, "int < for >")) {
            if (!run_for(lines, &i, end)) {
                novac_report_error(i + 1, "invalid for block", line);
                return 0;
            }
        } else if (starts_with(line, "elif ") || starts_with(line, ">> func << else")) {
            continue;
        } else if (run_assign(line)) {
            continue;
        } else {
            novac_report_error(i + 1, "unknown instruction", line);
            return 0;
        }
    }
    return 1;
}

void novac_init(void) {
    // Runtime is ready. Real compiler comes later.
}

void novac_help(void) {
    vga_set_color(0x0E);
    vga_writeln("NovaC inside NovaOS");
    vga_set_color(0x0F);
    vga_writeln("Official syntax now:");
    vga_writeln("  var name = \"text\"");
    vga_writeln("  var n = 2 + 3");
    vga_writeln("  n = n + 1");
    vga_writeln("  input(name)");
    vga_writeln("  theme(0..3) changes desktop theme");
    vga_writeln("  int << func >>(name)");
    vga_writeln("  << ! >func> if n >= 2 { ... } elif n != 0 { ... } >> func << else { ... }");
    vga_writeln("  <<While>>! <on> n < 5 { ... }");
    vga_writeln("  int < for > i in 5 { ... }");
    vga_writeln("  Conditions: == != > < >= <= and or not");
}

int novac_run_source(const char* source) {
    source = skip_spaces(source);
    if (!source || !source[0]) {
        vga_writeln("[NovaC] Empty source.");
        return 0;
    }

    clear_vars();
    novac_error_reported = 0;
    char lines[NOVAC_MAX_LINES][NOVAC_LINE_LEN];
    int count = split_lines(source, lines);
    return run_lines(lines, 0, count);
}
