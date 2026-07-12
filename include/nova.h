#ifndef NOVA_H
#define NOVA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int i32;

#define NOVA_KEY_UP 1
#define NOVA_KEY_DOWN 2
#define NOVA_KEY_LEFT 3
#define NOVA_KEY_RIGHT 4

void outb(u16 port, u8 value);
void outw(u16 port, u16 value);
u8 inb(u16 port);
u16 inw(u16 port);

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
int storage_format_novafs(void);
int storage_read_novafs(u8* buffer, int bytes);
int storage_write_novafs(const u8* buffer, int bytes);

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
int ramfs_set_current(int id);
int ramfs_total_slots(void);
int ramfs_entry_used(int id);
int ramfs_entry_parent(int id);
const char* ramfs_entry_name(int id);
int ramfs_entry_is_dir(int id);
int ramfs_child_count(void);
const char* ramfs_child_name(int index);
int ramfs_child_is_dir(int index);
int ramfs_save_to_storage(void);
int ramfs_load_from_storage(void);

void kernel_panic(const char* message, const char* file, int line);
void interrupts_init(void);
void system_reboot(void);
void system_shutdown(void);
void desktop_set_theme(int theme);
int desktop_get_theme(void);
const char* desktop_theme_name(void);

void novac_init(void);
void novac_help(void);
int novac_run_source(const char* source);

#define NOVA_PANIC(message) kernel_panic((message), __FILE__, __LINE__)

// ── POKEY Audio Emulator ───────────────────────────────────
void pokey_init(void);
void pokey_set_audf(int channel, u8 value);
void pokey_set_audc(int channel, u8 value);
void pokey_set_audctl(u8 value);
void pokey_set_skctl(u8 value);
void pokey_stimer(void);
u8   pokey_get_volume(int channel);
u32  pokey_get_frequency(int channel);
u8   pokey_freq_to_audf(u32 freq_hz, int use_179mhz);
void pokey_play(u32 duration_ms);
void pokey_stop_all(void);
void pokey_play_note(u32 freq, u32 ms, u8 distortion, u8 volume);
void pokey_boot_jingle(void);
void pokey_beep(void);
void pokey_error_sound(void);
void pokey_laser_sound(void);
void pokey_explosion_sound(void);
void pokey_powerup_sound(void);
void pokey_missile_command_theme(void);
void pokey_status(void);

#ifdef __cplusplus
}
#endif

#endif
