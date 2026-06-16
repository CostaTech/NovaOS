; NovaOS boot entry.
; GRUB loads us through the Multiboot protocol.
; Assembly is kept tiny on purpose: setup stack, pass boot info to C++.

BITS 32

MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 0x00000003        ; align modules + memory info
MB_CHECK equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    push ebx                  ; multiboot info pointer
    push eax                  ; multiboot magic
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
