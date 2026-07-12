#include "nova.h"

void desktop_loop();
void ramfs_init();
void novac_init();

static char active_user[32] = "guest";

void session_set_username(const char *name)
{
    int i = 0;
    if (!name || !name[0])
        name = "guest";
    while (name[i] && i < 31)
    {
        active_user[i] = name[i];
        i++;
    }
    active_user[i] = 0;
}

const char *session_username(void)
{
    return active_user;
}

static void login_screen()
{
    char username[32];
    int len = 0;
    username[0] = 0;

    vga_clear(0x10);
    vga_write_at(0, 0, "================================================================================", 0x30);
    vga_write_at(2, 1, "NOVA OS // LOGIN STATION", 0x3E);
    vga_write_at(57, 1, "Boot: LIVE", 0x3B);

    vga_write_at(18, 4, "+--------------------------------------------+", 0x1B);
    vga_write_at(18, 5, "|              NovaOS Account               |", 0x1E);
    vga_write_at(18, 6, "+--------------------------------------------+", 0x1B);
    vga_write_at(20, 8, "User name", 0x1A);
    vga_write_at(20, 9, "----------------------------------------", 0x18);
    vga_write_at(20, 11, "Press ENTER to start. Empty name = guest.", 0x1E);
    vga_write_at(20, 9, "> ", 0x1F);

    for (;;)
    {
        char c = keyboard_read_char();
        if (c == '\n')
        {
            if (len == 0)
            {
                username[0] = 'g';
                username[1] = 'u';
                username[2] = 'e';
                username[3] = 's';
                username[4] = 't';
                username[5] = 0;
            }
            break;
        }
        if (c == '\b')
        {
            if (len > 0)
            {
                len--;
                username[len] = 0;
                vga_write_at(22, 9, "_______________________________", 0x18);
                vga_write_at(22, 9, username, 0x1F);
            }
        }
        else if (c >= 32 && c <= 126 && len < 31)
        {
            username[len++] = c;
            username[len] = 0;
            vga_write_at(22, 9, "_______________________________", 0x18);
            vga_write_at(22, 9, username, 0x1F);
        }
    }

    session_set_username(username);

    vga_clear(0x10);
    vga_write_at(22, 10, "Welcome to NovaOS,", 0x1E);
    vga_write_at(41, 10, username, 0x1B);
    vga_write_at(22, 12, "Loading galactic desktop...", 0x1A);
    for (int i = 0; i < 8000000; i++)
        __asm__ volatile("nop");
}

extern "C" void kernel_main(u32 magic, u32 mbi_addr)
{
    if (magic != 0x2BADB002)
    {
        NOVA_PANIC("Invalid Multiboot magic. Bootloader did not load NovaOS correctly.");
    }
    interrupts_init();
    pokey_init();        // ← dopo interrupts
    pokey_boot_jingle(); // ← jingle di boot
    framebuffer_init(magic, mbi_addr);
    keyboard_init();
    mouse_init();
    storage_init();
    ramfs_init();
    novac_init();
    login_screen();
    desktop_loop();
    for (;;)
        __asm__ volatile("hlt");
}
