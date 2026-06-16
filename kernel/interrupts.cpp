#include "nova.h"

struct IdtEntry {
    u16 base_low;
    u16 selector;
    u8 zero;
    u8 flags;
    u16 base_high;
} __attribute__((packed));

struct IdtPointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct InterruptFrame {
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 interrupt_number;
    u32 error_code;
    u32 eip;
    u32 cs;
    u32 eflags;
};

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();

static IdtEntry idt[256];
static IdtPointer idt_ptr;

static const char* exception_name(u32 n) {
    static const char* names[32] = {
        "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
        "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
        "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
        "Stack segment fault", "General protection fault", "Page fault", "Reserved",
        "x87 floating point fault", "Alignment check", "Machine check", "SIMD floating point fault",
        "Virtualization fault", "Control protection fault", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor injection", "VMM communication fault", "Security exception", "Reserved"
    };
    if (n < 32) return names[n];
    return "Unknown CPU exception";
}

static void idt_set_gate(int index, u32 base) {
    idt[index].base_low = (u16)(base & 0xFFFF);
    idt[index].selector = 0x08;
    idt[index].zero = 0;
    idt[index].flags = 0x8E;
    idt[index].base_high = (u16)((base >> 16) & 0xFFFF);
}

static void print_hex(u32 value) {
    const char* hex = "0123456789ABCDEF";
    vga_write("0x");
    for (int i = 7; i >= 0; i--) {
        vga_putc(hex[(value >> (i * 4)) & 0xF]);
    }
}

extern "C" void cpu_exception_handler(InterruptFrame* frame) {
    __asm__ volatile ("cli");

    vga_clear(0x4F);
    vga_write_at(2, 2, "NOVAOS KERNEL PANIC", 0x4F);
    vga_write_at(2, 4, "Unhandled CPU exception.", 0x4F);

    vga_write_at(2, 7, "Exception:", 0x4E);
    vga_write_at(13, 7, exception_name(frame->interrupt_number), 0x4F);

    vga_set_color(0x4F);
    vga_write_at(2, 9, "Vector:", 0x4E);
    vga_set_color(0x4F);
    print_hex(frame->interrupt_number);

    vga_write_at(2, 10, "Error:", 0x4E);
    vga_set_color(0x4F);
    print_hex(frame->error_code);

    vga_write_at(2, 11, "EIP:", 0x4E);
    vga_set_color(0x4F);
    print_hex(frame->eip);

    vga_write_at(2, 14, "Restart the VM/PC to continue.", 0x4F);

    for (;;) __asm__ volatile ("hlt");
}

extern "C" void interrupts_init(void) {
    for (int i = 0; i < 256; i++) idt_set_gate(i, 0);

    void (*handlers[32])() = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) idt_set_gate(i, (u32)handlers[i]);

    idt_ptr.limit = (u16)(sizeof(idt) - 1);
    idt_ptr.base = (u32)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}
