/* ramfs.c - NovaFS v2 - RAM Filesystem for NovaOS */
/* CostaTech - 2026 */

#include "nova.h"

/* =====================================================================
 * CONFIGURAZIONE
 * ===================================================================== */

#define MAX_ENTRIES       128
#define MAX_NAME_LEN      64
#define BLOCK_SIZE        512
#define MAX_BLOCKS        8           /* file max 4KB */
#define NUM_DATA_BLOCKS   512         /* 512 * 512 = 256KB pool dati */
#define MAX_PATH_DEPTH    32
#define MAX_PATH_LEN      256

/* =====================================================================
 * TIPI
 * ===================================================================== */

typedef struct {
    int used;
    int is_dir;
    int protected_entry;
    int parent;
    char name[MAX_NAME_LEN];
    unsigned int size;
    unsigned int created_tick;
    unsigned int modified_tick;
    int blocks[MAX_BLOCKS];   /* -1 = non allocato */
} RamEntry;

/* =====================================================================
 * STATO GLOBALE
 * ===================================================================== */

static RamEntry entries[MAX_ENTRIES];
static char data_blocks[NUM_DATA_BLOCKS][BLOCK_SIZE];
static int block_used[NUM_DATA_BLOCKS];
static int cwd = 0;
static char path_buffer[MAX_PATH_LEN];
static char read_buffer[4096];  /* Buffer per ramfs_read_file compatibile */

/* get_system_tick è definita in interrupts.c */
extern unsigned int get_system_tick(void);

/* =====================================================================
 * STRINGHE (static, no libc)
 * ===================================================================== */

static int str_len(const char* s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static int str_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

/* =====================================================================
 * VALIDAZIONE
 * ===================================================================== */

static int valid_name(const char* name) {
    int n = str_len(name);
    if (n <= 0 || n >= MAX_NAME_LEN) return 0;
    if (str_eq(name, ".") || str_eq(name, "..") || str_eq(name, "/")) return 0;
    for (int i = 0; i < n; i++) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
                 c == ' ';
        if (!ok) return 0;
    }
    return 1;
}

/* =====================================================================
 * BLOCCHI DATI
 * ===================================================================== */

static void blocks_init(void) {
    for (int i = 0; i < NUM_DATA_BLOCKS; i++) {
        block_used[i] = 0;
        for (int j = 0; j < BLOCK_SIZE; j++) {
            data_blocks[i][j] = 0;
        }
    }
}

static int block_alloc(void) {
    for (int i = 0; i < NUM_DATA_BLOCKS; i++) {
        if (!block_used[i]) {
            block_used[i] = 1;
            for (int j = 0; j < BLOCK_SIZE; j++) data_blocks[i][j] = 0;
            return i;
        }
    }
    return -1;
}

static void block_free(int b) {
    if (b >= 0 && b < NUM_DATA_BLOCKS) {
        block_used[b] = 0;
        for (int j = 0; j < BLOCK_SIZE; j++) data_blocks[b][j] = 0;
    }
}

static void blocks_free_all(int* blocks) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i] >= 0) {
            block_free(blocks[i]);
            blocks[i] = -1;
        }
    }
}

/* =====================================================================
 * ENTRY MANAGEMENT
 * ===================================================================== */

static void entry_clear(int id) {
    entries[id].used = 0;
    entries[id].is_dir = 0;
    entries[id].protected_entry = 0;
    entries[id].parent = 0;
    entries[id].name[0] = 0;
    entries[id].size = 0;
    entries[id].created_tick = 0;
    entries[id].modified_tick = 0;
    for (int i = 0; i < MAX_BLOCKS; i++) entries[id].blocks[i] = -1;
}

static void entries_clear_all(void) {
    for (int i = 0; i < MAX_ENTRIES; i++) entry_clear(i);
}

static int find_child(int parent, const char* name) {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (entries[i].used && entries[i].parent == parent && str_eq(entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int alloc_entry(void) {
    for (int i = 1; i < MAX_ENTRIES; i++) {
        if (!entries[i].used) {
            entry_clear(i);
            return i;
        }
    }
    return -1;
}

static int has_children(int parent_id) {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (entries[i].used && entries[i].parent == parent_id && i != parent_id)
            return 1;
    }
    return 0;
}

/* =====================================================================
 * LETTURA/SCRITTURA FILE (multi-blocco)
 * ===================================================================== */

static int file_read_byte(int entry_id, unsigned int offset, char* out) {
    if (offset >= entries[entry_id].size) return 0;
    int block_idx = offset / BLOCK_SIZE;
    int block_off = offset % BLOCK_SIZE;
    int b = entries[entry_id].blocks[block_idx];
    if (b < 0) return 0;
    *out = data_blocks[b][block_off];
    return 1;
}

static int file_write_byte(int entry_id, unsigned int offset, char c) {
    if (offset >= MAX_BLOCKS * BLOCK_SIZE) return 0;
    int block_idx = offset / BLOCK_SIZE;
    int block_off = offset % BLOCK_SIZE;

    if (entries[entry_id].blocks[block_idx] < 0) {
        int b = block_alloc();
        if (b < 0) return 0;
        entries[entry_id].blocks[block_idx] = b;
    }

    data_blocks[entries[entry_id].blocks[block_idx]][block_off] = c;
    return 1;
}

/* =====================================================================
 * CREAZIONE ENTRY
 * ===================================================================== */

static int make_entry_ex(int parent, const char* name, int is_dir,
                         const char* content, int protected_entry) {
    if (!valid_name(name)) return 0;
    if (find_child(parent, name) >= 0) return 0;
    int id = alloc_entry();
    if (id < 0) return 0;

    unsigned int tick = get_system_tick();

    entries[id].used = 1;
    entries[id].is_dir = is_dir;
    entries[id].protected_entry = protected_entry;
    entries[id].parent = parent;
    str_copy(entries[id].name, name, MAX_NAME_LEN);
    entries[id].size = 0;
    entries[id].created_tick = tick;
    entries[id].modified_tick = tick;
    for (int i = 0; i < MAX_BLOCKS; i++) entries[id].blocks[i] = -1;

    if (!is_dir && content && content[0]) {
        int len = str_len(content);
        if (len > MAX_BLOCKS * BLOCK_SIZE) len = MAX_BLOCKS * BLOCK_SIZE;
        for (int i = 0; i < len; i++) {
            if (!file_write_byte(id, i, content[i])) {
                entry_clear(id);
                return 0;
            }
        }
        entries[id].size = len;
    }

    return 1;
}

static int make_entry(int parent, const char* name, int is_dir, const char* content) {
    return make_entry_ex(parent, name, is_dir, content, 0);
}

static int make_system_entry(int parent, const char* name, int is_dir, const char* content) {
    return make_entry_ex(parent, name, is_dir, content, 1);
}

/* =====================================================================
 * INIT
 * ===================================================================== */

void ramfs_init(void) {
    entries_clear_all();
    blocks_init();

    entries[0].used = 1;
    entries[0].is_dir = 1;
    entries[0].protected_entry = 1;
    entries[0].parent = 0;
    str_copy(entries[0].name, "/", MAX_NAME_LEN);
    cwd = 0;

    make_system_entry(0, "system", 1, "");
    make_system_entry(0, "apps", 1, "");
    make_system_entry(0, "home", 1, "");
    make_system_entry(0, "docs", 1, "");
    make_system_entry(0, "games", 1, "");
    make_system_entry(0, "images", 1, "");
    make_system_entry(0, "readme.txt", 0,
        "NovaOS RAM filesystem v2 is active. Type help in Terminal.");

    int docs = find_child(0, "docs");
    int home = find_child(0, "home");
    int apps = find_child(0, "apps");
    int games = find_child(0, "games");
    int images = find_child(0, "images");

    if (docs >= 0) {
        make_system_entry(docs, "about.txt", 0,
            "NovaOS is created by CostaTech. It has Desktop, Terminal, "
            "NovaFS v2, NovaC apps and protected official files.");
        make_system_entry(docs, "lnp.txt", 0,
            "LNP is Nova Picture. Use lnpinfo galaxy.lnp for metadata.");
        make_system_entry(docs, "novac.txt", 0,
            "NovaC uses var, input, int << func >>, special if and special while.");
        make_system_entry(docs, "games.txt", 0,
            "Games will be launched from /games and written in NovaC.");
        make_system_entry(docs, "tour.txt", 0,
            "NovaOS Tour: open Files, Terminal, Settings, Calculator, Galaxy and Games.");
    }
    if (home >= 0) make_system_entry(home, "notes.txt", 0,
        "Write your first NovaOS files here.");
    if (apps >= 0) {
        make_system_entry(apps, "settings.nc", 0,
            "int << func >>(\"NovaOS Settings\")\n"
            "int << func >>(\"Desktop theme\")\n"
            "int << func >>(\"0 Blue Galaxy\")\n"
            "int << func >>(\"1 Nebula Green\")\n"
            "int << func >>(\"2 Violet Orbit\")\n"
            "int << func >>(\"3 Deep Space\")\n"
            "int << func >>(\"Choose theme number:\")\n"
            "input(choice)\n"
            "theme(choice)\n"
            "int << func >>(\"Return to desktop to see it.\")");
        make_system_entry(apps, "tour.nc", 0,
            "int << func >>(\"Welcome to NovaOS\")\n"
            "int << func >>(\"1 Files: explore NovaFS\")\n"
            "int << func >>(\"2 Terminal: run commands\")\n"
            "int << func >>(\"3 NovaC: build OS apps\")\n"
            "int << func >>(\"4 Games and graphics will grow here\")\n"
            "int << func >>(\"5 Use apps and runNC to launch programs\")");
        make_system_entry(apps, "documentation.nc", 0,
            "int << func >>(\"NovaOS Documentation\")\n"
            "int << func >>(\"help core, help fs, help lang, help apps\")\n"
            "int << func >>(\"Official NovaC syntax is special\")\n"
            "int << func >>(\"Use cd apps, runNC name.nc, newnc file.nc\")\n"
            "int << func >>(\"Default files are protected\")");
        make_system_entry(apps, "notes.nc", 0,
            "int << func >>(\"Nova Notes\")\n"
            "int << func >>(\"Use edit filename.txt from Terminal to write notes.\")\n"
            "int << func >>(\"User files live in /home.\")");
        make_system_entry(apps, "sysinfo.nc", 0,
            "int << func >>(\"NovaSys Info\")\n"
            "int << func >>(\"Kernel: NovaOS C/C++ core\")\n"
            "int << func >>(\"Apps: NovaC protected files\")\n"
            "int << func >>(\"Desktop: mouse driven VGA mode\")");
        make_system_entry(apps, "shellhelp.nc", 0,
            "int << func >>(\"Shell Quick Help\")\n"
            "int << func >>(\"apps = list official apps\")\n"
            "int << func >>(\"runNC calculator.nc = open calculator\")\n"
            "int << func >>(\"newnc myapp.nc = create app file\")\n"
            "int << func >>(\"runNC file.nc = run current file\")");
        make_system_entry(apps, "devguide.nc", 0,
            "int << func >>(\"Nova Dev Guide\")\n"
            "int << func >>(\"Use var for data\")\n"
            "int << func >>(\"Use input(name) for keyboard input\")\n"
            "int << func >>(\"Use int << func >>(name) to print\")\n"
            "int << func >>(\"Use the special if and while syntax\")");
        make_system_entry(apps, "mouseinfo.nc", 0,
            "int << func >>(\"Mouse Info\")\n"
            "int << func >>(\"NovaOS uses a PS/2 polling mouse driver.\")\n"
            "int << func >>(\"The driver is filtered for real hardware tests.\")");
        make_system_entry(apps, "storageinfo.nc", 0,
            "int << func >>(\"Storage Info\")\n"
            "int << func >>(\"NovaFS v2 uses multi-block architecture.\")\n"
            "int << func >>(\"Official apps are protected and restored at boot.\")");
        make_system_entry(apps, "theme.nc", 0,
            "int << func >>(\"Nova Theme\")\n"
            "int << func >>(\"Blue space shell, yellow highlights, cyan links.\")\n"
            "int << func >>(\"Next step: real theme settings.\")");
        make_system_entry(apps, "clock.nc", 0,
            "int << func >>(\"Nova Clock\")\n"
            "int << func >>(\"RTC driver will arrive later.\")\n"
            "int << func >>(\"For now this is a NovaC placeholder app.\")");
        make_system_entry(apps, "syntax_test.nc", 0,
            "int << func >>(\"NovaC Syntax Test\")\n"
            "var x = 0\n"
            "x = x + 1\n"
            "int << func >>(x)\n"
            "<< ! >func> if x >= 2 {\n"
            "int << func >>(\"if branch\")\n"
            "} elif x != 0 and not x == 3 {\n"
            "int << func >>(\"elif branch\")\n"
            "} >> func << else {\n"
            "int << func >>(\"else branch\")\n"
            "}\n"
            "<<While>>! <on> x < 3 {\n"
            "int << func >>(x)\n"
            "x = x + 1\n"
            "}\n"
            "int < for > i in 3 {\n"
            "int << func >>(i)\n"
            "}");
        make_system_entry(apps, "calculator.nc", 0,
            "int << func >>(\"NovaCalc\")\n"
            "int << func >>(\"First number:\")\n"
            "input(a)\n"
            "int << func >>(\"Operator + - * /:\")\n"
            "input(op)\n"
            "int << func >>(\"Second number:\")\n"
            "input(b)\n"
            "<< ! >func> if op == \"+\" {\n"
            "var result = a + b\n"
            "int << func >>(result)\n"
            "} elif op == \"-\" {\n"
            "var result = a - b\n"
            "int << func >>(result)\n"
            "} elif op == \"*\" {\n"
            "var result = a * b\n"
            "int << func >>(result)\n"
            "} elif op == \"/\" {\n"
            "var result = a / b\n"
            "int << func >>(result)\n"
            "} >> func << else {\n"
            "int << func >>(\"Unknown operator\")\n"
            "}");
    }
        if (games >= 0) {
        make_system_entry(games, "hello_game.nc", 0,
            "var title = \"Nova Games will run NovaC apps\"\n"
            "int << func >>(title)");
        make_system_entry(games, "nova_quest.nc", 0,
            "int << func >>(\"Nova Quest\")\n"
            "var score = 0\n"
            "int << func >>(\"Dark satellite\")\n"
            "int << func >>(\"1 dock 2 scan\")\n"
            "input(c)\n"
            "<< ! >func> if c == \"1\" {\n"
            "score = score + 2\n"
            "int << func >>(\"Docked\")\n"
            "} elif c == \"2\" {\n"
            "score = score + 1\n"
            "int << func >>(\"Signal\")\n"
            "} >> func << else {\n"
            "score = score - 1\n"
            "int << func >>(\"Lost\")\n"
            "}\n"
            "int << func >>(\"Door 1 key 2 force\")\n"
            "input(c)\n"
            "<< ! >func> if c == \"1\" and score >= 1 {\n"
            "score = score + 4\n"
            "int << func >>(\"Opened\")\n"
            "} elif c == \"2\" {\n"
            "score = score + 1\n"
            "int << func >>(\"Alarm\")\n"
            "} >> func << else {\n"
            "score = score - 2\n"
            "int << func >>(\"Locked\")\n"
            "}\n"
            "int << func >>(\"Escape 1 launch 2 repair\")\n"
            "input(c)\n"
            "<< ! >func> if c == \"2\" {\n"
            "score = score + 3\n"
            "int << func >>(\"Stable\")\n"
            "} elif c == \"1\" {\n"
            "score = score + 1\n"
            "int << func >>(\"Risky\")\n"
            "} >> func << else {\n"
            "score = score - 1\n"
            "int << func >>(\"No action\")\n"
            "}\n"
            "<< ! >func> if score >= 6 {\n"
            "int << func >>(\"MISSION COMPLETE\")\n"
            "} elif score >= 2 {\n"
            "int << func >>(\"ESCAPED\")\n"
            "} >> func << else {\n"
            "int << func >>(\"MISSION FAILED\")\n"
            "}\n"
            "int << func >>(score)");
        make_system_entry(games, "README.txt", 0,
            "Put future NovaC games here.");
    }
    if (images >= 0) {
        make_system_entry(images, "galaxy.lnp", 0,
            "LNP1 64 36 RGB Nova placeholder");
    }
}

/* =====================================================================
 * API PUBBLICA
 * ===================================================================== */

const char* ramfs_pwd(void) {
    if (cwd == 0) {
        path_buffer[0] = '/';
        path_buffer[1] = 0;
        return path_buffer;
    }

    const char* parts[MAX_PATH_DEPTH];
    int count = 0;
    int cur = cwd;
    while (cur != 0 && count < MAX_PATH_DEPTH) {
        parts[count++] = entries[cur].name;
        cur = entries[cur].parent;
    }

    int pos = 0;
    path_buffer[pos++] = '/';
    for (int i = count - 1; i >= 0; i--) {
        const char* name = parts[i];
        for (int j = 0; name[j] && pos < MAX_PATH_LEN - 2; j++)
            path_buffer[pos++] = name[j];
        if (i > 0 && pos < MAX_PATH_LEN - 2)
            path_buffer[pos++] = '/';
    }
    path_buffer[pos] = 0;
    return path_buffer;
}

int ramfs_mkdir(const char* name) {
    return make_entry(cwd, name, 1, "");
}

int ramfs_create_file(const char* name, const char* content) {
    return make_entry(cwd, name, 0, content);
}

int ramfs_cd(const char* name) {
    if (!name || !name[0]) return 0;
    if (str_eq(name, "/")) {
        cwd = 0;
        return 1;
    }
    if (str_eq(name, "..")) {
        if (cwd != 0) cwd = entries[cwd].parent;
        return 1;
    }
    int id = find_child(cwd, name);
    if (id < 0 || !entries[id].is_dir) return 0;
    cwd = id;
    return 1;
}

int ramfs_rm(const char* name) {
    int id = find_child(cwd, name);
    if (id <= 0) return 0;
    if (entries[id].protected_entry) return 0;
    if (entries[id].is_dir && has_children(id)) return 0;

    blocks_free_all(entries[id].blocks);
    entry_clear(id);
    return 1;
}

int ramfs_rename(const char* old_name, const char* new_name) {
    if (!valid_name(new_name)) return 0;
    int id = find_child(cwd, old_name);
    if (id <= 0) return 0;
    if (entries[id].protected_entry) return 0;
    if (find_child(cwd, new_name) >= 0) return 0;
    str_copy(entries[id].name, new_name, MAX_NAME_LEN);
    entries[id].modified_tick = get_system_tick();
    return 1;
}

/* Compatibile con nova.h: ritorna const char* */
const char* ramfs_read_file(const char* name) {
    int id = find_child(cwd, name);
    if (id < 0 || entries[id].is_dir) return 0;

    int to_read = entries[id].size;
    if (to_read > 4095) to_read = 4095;

    for (int i = 0; i < to_read; i++) {
        char c;
        if (!file_read_byte(id, i, &c)) break;
        read_buffer[i] = c;
    }
    read_buffer[to_read] = 0;
    return read_buffer;
}

int ramfs_write_file(const char* name, const char* content) {
    int id = find_child(cwd, name);
    if (id < 0 || entries[id].is_dir) return 0;
    if (entries[id].protected_entry) return 0;

    blocks_free_all(entries[id].blocks);
    entries[id].size = 0;

    if (!content) {
        entries[id].modified_tick = get_system_tick();
        return 1;
    }

    int len = str_len(content);
    if (len > MAX_BLOCKS * BLOCK_SIZE) len = MAX_BLOCKS * BLOCK_SIZE;

    for (int i = 0; i < len; i++) {
        if (!file_write_byte(id, i, content[i])) {
            blocks_free_all(entries[id].blocks);
            entries[id].size = 0;
            return 0;
        }
    }

    entries[id].size = len;
    entries[id].modified_tick = get_system_tick();
    return 1;
}

int ramfs_append_file(const char* name, const char* content) {
    int id = find_child(cwd, name);
    if (id < 0 || entries[id].is_dir) return 0;
    if (entries[id].protected_entry) return 0;
    if (!content) return 1;

    int len = str_len(content);
    unsigned int start = entries[id].size;
    if (start + len > MAX_BLOCKS * BLOCK_SIZE)
        len = MAX_BLOCKS * BLOCK_SIZE - start;

    for (int i = 0; i < len; i++) {
        if (!file_write_byte(id, start + i, content[i])) return 0;
    }

    entries[id].size = start + len;
    entries[id].modified_tick = get_system_tick();
    return 1;
}

/* =====================================================================
 * DIRECTORY LISTING
 * ===================================================================== */

static int child_index(int visible_index) {
    int seen = 0;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (entries[i].used && entries[i].parent == cwd && i != cwd) {
            if (seen == visible_index) return i;
            seen++;
        }
    }
    return -1;
}

int ramfs_child_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (entries[i].used && entries[i].parent == cwd && i != cwd) count++;
    }
    return count;
}

const char* ramfs_child_name(int index) {
    int id = child_index(index);
    if (id < 0) return "";
    return entries[id].name;
}

int ramfs_child_is_dir(int index) {
    int id = child_index(index);
    if (id < 0) return 0;
    return entries[id].is_dir;
}

/* =====================================================================
 * METADATA ACCESS
 * ===================================================================== */

int ramfs_current_id(void) { return cwd; }

int ramfs_set_current(int id) {
    if (id < 0 || id >= MAX_ENTRIES || !entries[id].used || !entries[id].is_dir) return 0;
    cwd = id;
    return 1;
}

int ramfs_total_slots(void) { return MAX_ENTRIES; }

int ramfs_entry_used(int id) {
    if (id < 0 || id >= MAX_ENTRIES) return 0;
    return entries[id].used;
}

int ramfs_entry_parent(int id) {
    if (id < 0 || id >= MAX_ENTRIES) return -1;
    return entries[id].parent;
}

const char* ramfs_entry_name(int id) {
    if (id < 0 || id >= MAX_ENTRIES || !entries[id].used) return "";
    return entries[id].name;
}

int ramfs_entry_is_dir(int id) {
    if (id < 0 || id >= MAX_ENTRIES || !entries[id].used) return 0;
    return entries[id].is_dir;
}

/* =====================================================================
 * PERSISTENZA — NOVAFS2 FORMAT
 * ===================================================================== */

#define NOVAFS2_HEADER_SIZE 512

static u8 novafs_image[NOVAFS2_HEADER_SIZE + MAX_ENTRIES * 256 + NUM_DATA_BLOCKS * BLOCK_SIZE];

static void image_clear(void) {
    for (unsigned int i = 0; i < sizeof(novafs_image); i++) novafs_image[i] = 0;
}

static void image_put32(int pos, unsigned int value) {
    novafs_image[pos]     = (u8)(value & 0xFF);
    novafs_image[pos + 1] = (u8)((value >> 8) & 0xFF);
    novafs_image[pos + 2] = (u8)((value >> 16) & 0xFF);
    novafs_image[pos + 3] = (u8)((value >> 24) & 0xFF);
}

static unsigned int image_get32(int pos) {
    return (unsigned int)novafs_image[pos] |
           ((unsigned int)novafs_image[pos + 1] << 8) |
           ((unsigned int)novafs_image[pos + 2] << 16) |
           ((unsigned int)novafs_image[pos + 3] << 24);
}

static int image_has_magic(void) {
    return novafs_image[0] == 'N' && novafs_image[1] == 'O' &&
           novafs_image[2] == 'V' && novafs_image[3] == 'A' &&
           novafs_image[4] == 'F' && novafs_image[5] == 'S' &&
           novafs_image[6] == '2';
}

int ramfs_save_to_storage(void) {
    image_clear();
    novafs_image[0] = 'N'; novafs_image[1] = 'O'; novafs_image[2] = 'V';
    novafs_image[3] = 'A'; novafs_image[4] = 'F'; novafs_image[5] = 'S';
    novafs_image[6] = '2';
    image_put32(8, MAX_ENTRIES);
    image_put32(12, MAX_BLOCKS);
    image_put32(16, BLOCK_SIZE);
    image_put32(20, cwd);
    image_put32(24, get_system_tick());

    int base = NOVAFS2_HEADER_SIZE;
    for (int id = 0; id < MAX_ENTRIES; id++) {
        int pos = base + id * 256;
        if (!entries[id].used || entries[id].protected_entry) {
            image_put32(pos, 0);
            continue;
        }
        image_put32(pos,      entries[id].used);
        image_put32(pos + 4,  entries[id].is_dir);
        image_put32(pos + 8,  0);  /* reserved */
        image_put32(pos + 12, entries[id].parent);
        image_put32(pos + 16, entries[id].size);
        image_put32(pos + 20, entries[id].created_tick);
        image_put32(pos + 24, entries[id].modified_tick);

        for (int i = 0; i < MAX_BLOCKS; i++)
            image_put32(pos + 28 + i * 4, entries[id].blocks[i]);

        for (int i = 0; i < MAX_NAME_LEN && i < 64; i++)
            novafs_image[pos + 60 + i] = (u8)entries[id].name[i];
    }

    /* Salva blocchi dati */
    int data_base = base + MAX_ENTRIES * 256;
    for (int b = 0; b < NUM_DATA_BLOCKS; b++) {
        int img_pos = data_base + b * BLOCK_SIZE;
        if ((unsigned int)(img_pos + BLOCK_SIZE) > sizeof(novafs_image)) break;
        if (block_used[b]) {
            for (int j = 0; j < BLOCK_SIZE; j++)
                novafs_image[img_pos + j] = (u8)data_blocks[b][j];
        }
    }

    return storage_write_novafs(novafs_image, sizeof(novafs_image));
}

int ramfs_load_from_storage(void) {
    image_clear();
    if (!storage_read_novafs(novafs_image, sizeof(novafs_image))) return 0;
    if (!image_has_magic()) return 0;
    if (image_get32(8) != MAX_ENTRIES) return 0;
    if (image_get32(12) != MAX_BLOCKS) return 0;
    if (image_get32(16) != BLOCK_SIZE) return 0;

    /* Cancella solo entry utente (non protected) */
    for (int id = 1; id < MAX_ENTRIES; id++) {
        if (entries[id].used && !entries[id].protected_entry) {
            blocks_free_all(entries[id].blocks);
            entry_clear(id);
        }
    }

    int base = NOVAFS2_HEADER_SIZE;
    for (int id = 1; id < MAX_ENTRIES; id++) {
        int pos = base + id * 256;
        int used = image_get32(pos);
        if (!used) continue;

        int parent = image_get32(pos + 12);
        if (parent < 0 || parent >= MAX_ENTRIES || !entries[parent].used || !entries[parent].is_dir) continue;
        if (entries[id].used && entries[id].protected_entry) continue;

        entry_clear(id);
        entries[id].used = 1;
        entries[id].is_dir = image_get32(pos + 4);
        entries[id].protected_entry = 0;
        entries[id].parent = parent;
        entries[id].size = image_get32(pos + 16);
        entries[id].created_tick = image_get32(pos + 20);
        entries[id].modified_tick = image_get32(pos + 24);

        for (int i = 0; i < MAX_BLOCKS; i++)
            entries[id].blocks[i] = (int)image_get32(pos + 28 + i * 4);

        for (int i = 0; i < MAX_NAME_LEN && i < 64; i++)
            entries[id].name[i] = (char)novafs_image[pos + 60 + i];
        entries[id].name[MAX_NAME_LEN - 1] = 0;

        if (!valid_name(entries[id].name)) {
            entry_clear(id);
            continue;
        }
    }

    /* Ripristina blocchi dati */
    int data_base = base + MAX_ENTRIES * 256;
    for (int b = 0; b < NUM_DATA_BLOCKS; b++) {
        int img_pos = data_base + b * BLOCK_SIZE;
        if ((unsigned int)(img_pos + BLOCK_SIZE) > sizeof(novafs_image)) break;
        if (block_used[b]) {
            for (int j = 0; j < BLOCK_SIZE; j++)
                data_blocks[b][j] = (char)novafs_image[img_pos + j];
        }
    }

    int loaded_cwd = image_get32(20);
    if (loaded_cwd < 0 || loaded_cwd >= MAX_ENTRIES ||
        !entries[loaded_cwd].used || !entries[loaded_cwd].is_dir)
        loaded_cwd = 0;
    cwd = loaded_cwd;
    return entries[0].used && entries[0].is_dir;
}
