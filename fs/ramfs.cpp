#include "nova.h"

struct RamFile {
    int used;
    int is_dir;
    int protected_entry;
    int parent;
    char name[24];
    char content[512];
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
    str_copy(entries[id].content, content, 512);
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
    make_entry(0, "home", 1, "");
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
        make_system_entry(docs, "about.txt", 0, "NovaOS is created by CostaTech.");
        make_system_entry(docs, "lnp.txt", 0, "LNP is Nova Picture. Use lnpinfo galaxy.lnp for metadata.");
        make_system_entry(docs, "tencle.txt", 0, "TencleLang uses var name = \"text\" and int << func >>(name)");
        make_system_entry(docs, "games.txt", 0, "Games will be launched from /games and written in TencleLang.");
    }
    if (home >= 0) make_entry(home, "notes.txt", 0, "Write your first NovaOS files here.");
    if (apps >= 0) {
        make_system_entry(apps, "terminal.app", 0, "Terminal app entry.");
        make_system_entry(apps, "files.app", 0, "File manager entry.");
        make_system_entry(apps, "editor.app", 0, "NovaEdit entry.");
        make_system_entry(apps, "games.app", 0, "Games launcher entry.");
        make_system_entry(apps, "calculator.app", 0, "Calculator app entry.");
        make_system_entry(apps, "settings.tlang", 0, "var title = \"NovaOS Settings\"\nint << func >>(title)\nvar mouse = \"Mouse: PS/2 polling driver\"\nint << func >>(mouse)\nvar files = \"Default apps are protected\"\nint << func >>(files)");
        make_system_entry(apps, "hello.tlang", 0, "var msg = \"Hello from TencleLang inside NovaOS\"\nint << func >>(msg)");
        make_system_entry(apps, "calculator.tlang", 0, "var a = 8\nvar b = 2\nvar sum = a + b\nvar diff = a - b\nvar product = a * b\nvar division = a / b\nint << func >>(\"NovaCalc\")\nint << func >>(sum)\nint << func >>(diff)\nint << func >>(product)\n<< ! >func> if b > 0 {\nint << func >>(division)\n}\n>> func << else {\nint << func >>(\"Cannot divide\")\n}\nvar count = 0\n<<While>>! <on> count < 3 {\nint << func >>(count)\nvar count = count + 1\n}");
    }
    if (games >= 0) {
        make_system_entry(games, "hello_game.tlang", 0, "var title = \"Nova Games will run TencleLang apps\"\nint << func >>(title)");
        make_system_entry(games, "README.txt", 0, "Put future TencleLang games here.");
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
    str_copy(entries[id].content, content ? content : "", 512);
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
