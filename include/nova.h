#ifndef NOVA_H
#define NOVA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int i32;

void outb(u16 port, u8 value);
void outw(u16 port, u16 value);
u8 inb(u16 port);

void vga_clear(u8 color);
void vga_set_color(u8 color);
void vga_putc(char c);
void vga_write(const char* text);
void vga_writeln(const char* text);
void vga_write_at(int x, int y, const char* text, u8 color);

void keyboard_init(void);
char keyboard_read_char(void);
char keyboard_try_read_char(void);

void mouse_init(void);
void mouse_poll(void);
int mouse_x(void);
int mouse_y(void);
int mouse_buttons(void);
int mouse_enabled(void);
int mouse_is_ready(void);

void framebuffer_init(u32 magic, u32 mbi_addr);
int framebuffer_ready(void);
void framebuffer_clear(u32 color);
void framebuffer_rect(int x, int y, int w, int h, u32 color);
void framebuffer_demo(void);

void storage_init(void);
int storage_ready(void);
const char* storage_status_text(void);

void session_set_username(const char* name);
const char* session_username(void);

void ramfs_init(void);
const char* ramfs_pwd(void);
int ramfs_mkdir(const char* name);
int ramfs_create_file(const char* name, const char* content);
int ramfs_cd(const char* name);
int ramfs_rm(const char* name);
int ramfs_rename(const char* old_name, const char* new_name);
const char* ramfs_read_file(const char* name);
int ramfs_write_file(const char* name, const char* content);
int ramfs_current_id(void);
int ramfs_total_slots(void);
int ramfs_entry_used(int id);
int ramfs_entry_parent(int id);
const char* ramfs_entry_name(int id);
int ramfs_entry_is_dir(int id);
int ramfs_child_count(void);
const char* ramfs_child_name(int index);
int ramfs_child_is_dir(int index);

void kernel_panic(const char* message, const char* file, int line);
void interrupts_init(void);
void system_reboot(void);
void system_shutdown(void);

void tenclelang_init(void);
void tenclelang_help(void);
int tenclelang_run_source(const char* source);
#define NOVA_PANIC(message) kernel_panic((message), __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif
