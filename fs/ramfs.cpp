#include "nova.h"

struct RamFile {
    int used;
    int is_dir;
    int protected_entry;
    int parent;
    char name[24];
    char content[1024];
};

static RamFile entries[64];
static int cwd = 0;
static char path_buffer[128];

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

static int valid_name(const char* name) {
    int n = str_len(name);
    if (n <= 0 || n >= 24) return 0;
    if (str_eq(name, ".") || str_eq(name, "..") || str_eq(name, "/")) return 0;
    for (int i = 0; i < n; i++) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return 0;
    }
    return 1;
}

static int find_child(int parent, const char* name) {
    for (int i = 0; i < 64; i++) {
        if (entries[i].used && entries[i].parent == parent && str_eq(entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int alloc_entry() {
    for (int i = 1; i < 64; i++) {
        if (!entries[i].used) return i;
    }
    return -1;
}

static int make_entry_ex(int parent, const char* name, int is_dir, const char* content, int protected_entry) {
    if (!valid_name(name)) return 0;
    if (find_child(parent, name) >= 0) return 0;
    int id = alloc_entry();
    if (id < 0) return 0;

    entries[id].used = 1;
    entries[id].is_dir = is_dir;
    entries[id].protected_entry = protected_entry;
    entries[id].parent = parent;
    str_copy(entries[id].name, name, 24);
    str_copy(entries[id].content, content, 1024);
    return 1;
}

static int make_entry(int parent, const char* name, int is_dir, const char* content) {
    return make_entry_ex(parent, name, is_dir, content, 0);
}

static int make_system_entry(int parent, const char* name, int is_dir, const char* content) {
    return make_entry_ex(parent, name, is_dir, content, 1);
}

static void clear_entries() {
    for (int i = 0; i < 64; i++) {
        entries[i].used = 0;
        entries[i].is_dir = 0;
        entries[i].protected_entry = 0;
        entries[i].parent = 0;
        entries[i].name[0] = 0;
        entries[i].content[0] = 0;
    }
}

void ramfs_init(void) {
    clear_entries();

    entries[0].used = 1;
    entries[0].is_dir = 1;
    entries[0].protected_entry = 1;
    entries[0].parent = 0;
    str_copy(entries[0].name, "/", 24);
    cwd = 0;

    make_system_entry(0, "system", 1, "");
    make_system_entry(0, "apps", 1, "");
    make_system_entry(0, "home", 1, "");
    make_system_entry(0, "docs", 1, "");
    make_system_entry(0, "games", 1, "");
    make_system_entry(0, "images", 1, "");
    make_system_entry(0, "readme.txt", 0, "NovaOS RAM filesystem is active. Type help in Terminal.");

    int docs = find_child(0, "docs");
    int home = find_child(0, "home");
    int apps = find_child(0, "apps");
    int games = find_child(0, "games");
    int images = find_child(0, "images");
    if (docs >= 0) {
        make_system_entry(docs, "about.txt", 0, "NovaOS is created by CostaTech. It has Desktop, Terminal, NovaFS, NovaC apps and protected official files.");
        make_system_entry(docs, "lnp.txt", 0, "LNP is Nova Picture. Use lnpinfo galaxy.lnp for metadata.");
        make_system_entry(docs, "novac.txt", 0, "NovaC uses var, input, int << func >>, special if and special while. Normal print/if/while are not official.");
        make_system_entry(docs, "games.txt", 0, "Games will be launched from /games and written in NovaC.");
        make_system_entry(docs, "tour.txt", 0, "NovaOS Tour: open Files, Terminal, Settings, Calculator, Galaxy and Games from the desktop.");
    }
    if (home >= 0) make_system_entry(home, "notes.txt", 0, "Write your first NovaOS files here.");
    if (apps >= 0) {
        make_system_entry(apps, "settings.nc", 0, "int << func >>(\"NovaOS Settings\")\nint << func >>(\"Desktop theme\")\nint << func >>(\"0 Blue Galaxy\")\nint << func >>(\"1 Nebula Green\")\nint << func >>(\"2 Violet Orbit\")\nint << func >>(\"3 Deep Space\")\nint << func >>(\"Choose theme number:\")\ninput(choice)\ntheme(choice)\nint << func >>(\"Return to desktop to see it.\")");
        make_system_entry(apps, "tour.nc", 0, "int << func >>(\"Welcome to NovaOS\")\nint << func >>(\"1 Files: explore NovaFS\")\nint << func >>(\"2 Terminal: run commands\")\nint << func >>(\"3 NovaC: build OS apps\")\nint << func >>(\"4 Games and graphics will grow here\")\nint << func >>(\"5 Use apps and runNC to launch programs\")");
        make_system_entry(apps, "documentation.nc", 0, "int << func >>(\"NovaOS Documentation\")\nint << func >>(\"help core, help fs, help lang, help apps\")\nint << func >>(\"Official NovaC syntax is special\")\nint << func >>(\"Use cd apps, runNC name.nc, newnc file.nc\")\nint << func >>(\"Default files are protected\")");
        make_system_entry(apps, "paint.nc", 0, "int << func >>(\"NovaPaint\")\nint << func >>(\"+----------------+\")\nint << func >>(\"|  *   *   *     |\")\nint << func >>(\"|    NOVA ART    |\")\nint << func >>(\"|     *   *      |\")\nint << func >>(\"+----------------+\")");
        make_system_entry(apps, "notes.nc", 0, "int << func >>(\"Nova Notes\")\nint << func >>(\"Use edit filename.txt from Terminal to write notes.\")\nint << func >>(\"User files live in /home.\")");
        make_system_entry(apps, "hello.nc", 0, "var msg = \"Hello from NovaC inside NovaOS\"\nint << func >>(msg)");
        make_system_entry(apps, "sysinfo.nc", 0, "int << func >>(\"NovaSys Info\")\nint << func >>(\"Kernel: NovaOS C/C++ core\")\nint << func >>(\"Apps: NovaC protected files\")\nint << func >>(\"Desktop: mouse driven VGA mode\")");
        make_system_entry(apps, "shellhelp.nc", 0, "int << func >>(\"Shell Quick Help\")\nint << func >>(\"apps = list official apps\")\nint << func >>(\"runNC calculator.nc = open calculator\")\nint << func >>(\"newnc myapp.nc = create app file\")\nint << func >>(\"runNC file.nc = run current file\")");
        make_system_entry(apps, "devguide.nc", 0, "int << func >>(\"Nova Dev Guide\")\nint << func >>(\"Use var for data\")\nint << func >>(\"Use input(name) for keyboard input\")\nint << func >>(\"Use int << func >>(name) to print\")\nint << func >>(\"Use the special if and while syntax\")");
        make_system_entry(apps, "mouseinfo.nc", 0, "int << func >>(\"Mouse Info\")\nint << func >>(\"NovaOS uses a PS/2 polling mouse driver.\")\nint << func >>(\"The driver is filtered for real hardware tests.\")");
        make_system_entry(apps, "storageinfo.nc", 0, "int << func >>(\"Storage Info\")\nint << func >>(\"NovaFS is RAM based for now.\")\nint << func >>(\"Official apps are protected and restored at boot.\")");
        make_system_entry(apps, "theme.nc", 0, "int << func >>(\"Nova Theme\")\nint << func >>(\"Blue space shell, yellow highlights, cyan links.\")\nint << func >>(\"Next step: real theme settings.\")");
        make_system_entry(apps, "clock.nc", 0, "int << func >>(\"Nova Clock\")\nint << func >>(\"RTC driver will arrive later.\")\nint << func >>(\"For now this is a NovaC placeholder app.\")");
        make_system_entry(apps, "syntax_test.nc", 0, "int << func >>(\"NovaC Syntax Test\")\nvar x = 0\nx = x + 1\nint << func >>(x)\n<< ! >func> if x >= 2 {\nint << func >>(\"if branch\")\n} elif x != 0 and not x == 3 {\nint << func >>(\"elif branch\")\n} >> func << else {\nint << func >>(\"else branch\")\n}\n<<While>>! <on> x < 3 {\nint << func >>(x)\nx = x + 1\n}\nint < for > i in 3 {\nint << func >>(i)\n}");
        make_system_entry(apps, "calculator.nc", 0, "int << func >>(\"NovaCalc\")\nint << func >>(\"First number:\")\ninput(a)\nint << func >>(\"Operator + - * /:\")\ninput(op)\nint << func >>(\"Second number:\")\ninput(b)\n<< ! >func> if op == \"+\" {\nvar result = a + b\nint << func >>(result)\n} elif op == \"-\" {\nvar result = a - b\nint << func >>(result)\n} elif op == \"*\" {\nvar result = a * b\nint << func >>(result)\n} elif op == \"/\" {\nvar result = a / b\nint << func >>(result)\n} >> func << else {\nint << func >>(\"Unknown operator\")\n}");
    }
    if (games >= 0) {
        make_system_entry(games, "hello_game.nc", 0, "var title = \"Nova Games will run NovaC apps\"\nint << func >>(title)");
        make_system_entry(games, "nova_quest.nc", 0, "int << func >>(\"Nova Quest\")\nvar score = 0\nint << func >>(\"Dark satellite\")\nint << func >>(\"1 dock 2 scan\")\ninput(c)\n<< ! >func> if c == \"1\" {\nscore = score + 2\nint << func >>(\"Docked\")\n} elif c == \"2\" {\nscore = score + 1\nint << func >>(\"Signal\")\n} >> func << else {\nscore = score - 1\nint << func >>(\"Lost\")\n}\nint << func >>(\"Door 1 key 2 force\")\ninput(c)\n<< ! >func> if c == \"1\" and score >= 1 {\nscore = score + 4\nint << func >>(\"Opened\")\n} elif c == \"2\" {\nscore = score + 1\nint << func >>(\"Alarm\")\n} >> func << else {\nscore = score - 2\nint << func >>(\"Locked\")\n}\nint << func >>(\"Escape 1 launch 2 repair\")\ninput(c)\n<< ! >func> if c == \"2\" {\nscore = score + 3\nint << func >>(\"Stable\")\n} elif c == \"1\" {\nscore = score + 1\nint << func >>(\"Risky\")\n} >> func << else {\nscore = score - 1\nint << func >>(\"No action\")\n}\n<< ! >func> if score >= 6 {\nint << func >>(\"MISSION COMPLETE\")\n} elif score >= 2 {\nint << func >>(\"ESCAPED\")\n} >> func << else {\nint << func >>(\"MISSION FAILED\")\n}\nint << func >>(score)");
        make_system_entry(games, "README.txt", 0, "Put future NovaC games here.");
    }
    if (images >= 0) {
        make_system_entry(images, "galaxy.lnp", 0, "LNP1 64 36 RGB Nova placeholder");
    }
}

const char* ramfs_pwd(void) {
    if (cwd == 0) {
        path_buffer[0] = '/';
        path_buffer[1] = 0;
        return path_buffer;
    }

    const char* parts[12];
    int count = 0;
    int cur = cwd;
    while (cur != 0 && count < 12) {
        parts[count++] = entries[cur].name;
        cur = entries[cur].parent;
    }

    int pos = 0;
    path_buffer[pos++] = '/';
    for (int i = count - 1; i >= 0; i--) {
        const char* name = parts[i];
        for (int j = 0; name[j] && pos < 126; j++) path_buffer[pos++] = name[j];
        if (i > 0 && pos < 126) path_buffer[pos++] = '/';
    }
    path_buffer[pos] = 0;
    return path_buffer;
}

int ramfs_mkdir(const char* name) {
    return make_entry(cwd, name, 1, "");
}

int ramfs_create_file(const char* name, const char* content) {
    return make_entry(cwd, name, 0, content ? content : "");
}

int ramfs_cd(const char* name) {
    if (!name || !name[0]) return 0;
    if (str_eq(name, "/")) {
        cwd = 0;
        return 1;
    }
    if (str_eq(name, "..")) {
        cwd = entries[cwd].parent;
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
    if (entries[id].is_dir) {
        for (int i = 0; i < 64; i++) {
            if (entries[i].used && entries[i].parent == id) return 0;
        }
    }
    entries[id].used = 0;
    return 1;
}

int ramfs_rename(const char* old_name, const char* new_name) {
    if (!valid_name(new_name)) return 0;
    int id = find_child(cwd, old_name);
    if (id <= 0) return 0;
    if (entries[id].protected_entry) return 0;
    if (find_child(cwd, new_name) >= 0) return 0;
    str_copy(entries[id].name, new_name, 24);
    return 1;
}

const char* ramfs_read_file(const char* name) {
    int id = find_child(cwd, name);
    if (id < 0 || entries[id].is_dir) return 0;
    return entries[id].content;
}

int ramfs_write_file(const char* name, const char* content) {
    int id = find_child(cwd, name);
    if (id < 0 || entries[id].is_dir) return 0;
    if (entries[id].protected_entry) return 0;
    str_copy(entries[id].content, content ? content : "", 1024);
    return 1;
}

static int child_index(int visible_index) {
    int seen = 0;
    for (int i = 0; i < 64; i++) {
        if (entries[i].used && entries[i].parent == cwd && i != cwd) {
            if (seen == visible_index) return i;
            seen++;
        }
    }
    return -1;
}

int ramfs_child_count(void) {
    int count = 0;
    for (int i = 0; i < 64; i++) {
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

int ramfs_current_id(void) { return cwd; }

int ramfs_set_current(int id) {
    if (id < 0 || id >= 64 || !entries[id].used || !entries[id].is_dir) return 0;
    cwd = id;
    return 1;
}

int ramfs_total_slots(void) { return 64; }

int ramfs_entry_used(int id) {
    if (id < 0 || id >= 64) return 0;
    return entries[id].used;
}

int ramfs_entry_parent(int id) {
    if (id < 0 || id >= 64) return -1;
    return entries[id].parent;
}

const char* ramfs_entry_name(int id) {
    if (id < 0 || id >= 64 || !entries[id].used) return "";
    return entries[id].name;
}

int ramfs_entry_is_dir(int id) {
    if (id < 0 || id >= 64 || !entries[id].used) return 0;
    return entries[id].is_dir;
}


#define RAMFS_IMAGE_BYTES (144 * 512)
#define RAMFS_ENTRY_BYTES 1064

static u8 ramfs_image[RAMFS_IMAGE_BYTES];

static void image_clear(void) {
    for (int i = 0; i < RAMFS_IMAGE_BYTES; i++) ramfs_image[i] = 0;
}

static void image_put32(int pos, int value) {
    ramfs_image[pos] = (u8)(value & 0xFF);
    ramfs_image[pos + 1] = (u8)((value >> 8) & 0xFF);
    ramfs_image[pos + 2] = (u8)((value >> 16) & 0xFF);
    ramfs_image[pos + 3] = (u8)((value >> 24) & 0xFF);
}

static int image_get32(int pos) {
    return (int)ramfs_image[pos] |
           ((int)ramfs_image[pos + 1] << 8) |
           ((int)ramfs_image[pos + 2] << 16) |
           ((int)ramfs_image[pos + 3] << 24);
}

static int image_has_magic(void) {
    return ramfs_image[0] == 'N' && ramfs_image[1] == 'O' && ramfs_image[2] == 'V' &&
           ramfs_image[3] == 'A' && ramfs_image[4] == 'F' && ramfs_image[5] == 'S' &&
           ramfs_image[6] == '1';
}

int ramfs_save_to_storage(void) {
    image_clear();
    ramfs_image[0] = 'N';
    ramfs_image[1] = 'O';
    ramfs_image[2] = 'V';
    ramfs_image[3] = 'A';
    ramfs_image[4] = 'F';
    ramfs_image[5] = 'S';
    ramfs_image[6] = '1';
    image_put32(8, 64);
    image_put32(12, cwd);

    int base = 512;
    for (int id = 0; id < 64; id++) {
        int pos = base + id * RAMFS_ENTRY_BYTES;
        // Protected files are part of the kernel image, not user storage.
        if (!entries[id].used || entries[id].protected_entry) {
            image_put32(pos, 0);
            continue;
        }
        image_put32(pos, entries[id].used);
        image_put32(pos + 4, entries[id].is_dir);
        image_put32(pos + 8, 0);
        image_put32(pos + 12, entries[id].parent);
        for (int i = 0; i < 24; i++) ramfs_image[pos + 16 + i] = (u8)entries[id].name[i];
        for (int i = 0; i < 1024; i++) ramfs_image[pos + 40 + i] = (u8)entries[id].content[i];
    }

    return storage_write_novafs(ramfs_image, RAMFS_IMAGE_BYTES);
}

int ramfs_load_from_storage(void) {
    image_clear();
    if (!storage_read_novafs(ramfs_image, RAMFS_IMAGE_BYTES)) return 0;
    if (!image_has_magic()) return 0;
    if (image_get32(8) != 64) return 0;

    // Delete only user-created entries. Official protected files stay alive.
    for (int id = 1; id < 64; id++) {
        if (entries[id].used && !entries[id].protected_entry) {
            entries[id].used = 0;
            entries[id].is_dir = 0;
            entries[id].parent = 0;
            entries[id].name[0] = 0;
            entries[id].content[0] = 0;
        }
    }

    int base = 512;
    for (int id = 1; id < 64; id++) {
        int pos = base + id * RAMFS_ENTRY_BYTES;
        int used = image_get32(pos);
        if (!used) continue;

        int parent = image_get32(pos + 12);
        if (parent < 0 || parent >= 64 || !entries[parent].used || !entries[parent].is_dir) continue;
        if (entries[id].used && entries[id].protected_entry) continue;

        entries[id].used = 1;
        entries[id].is_dir = image_get32(pos + 4);
        entries[id].protected_entry = 0;
        entries[id].parent = parent;
        for (int i = 0; i < 24; i++) entries[id].name[i] = (char)ramfs_image[pos + 16 + i];
        entries[id].name[23] = 0;
        if (!valid_name(entries[id].name)) {
            entries[id].used = 0;
            continue;
        }
        for (int i = 0; i < 1024; i++) entries[id].content[i] = (char)ramfs_image[pos + 40 + i];
        entries[id].content[1023] = 0;
    }

    int loaded_cwd = image_get32(12);
    if (loaded_cwd < 0 || loaded_cwd >= 64 || !entries[loaded_cwd].used || !entries[loaded_cwd].is_dir) loaded_cwd = 0;
    cwd = loaded_cwd;
    return entries[0].used && entries[0].is_dir;
}

