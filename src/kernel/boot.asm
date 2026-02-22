[BITS 32]

; --- Multiboot Header ---
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x03
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kmain  ; Assure-toi que c'est bien le nom dans ton kernel.c

_start:
    mov esp, stack_top   
    
    push ebx             
    push eax             
    
    call kmain

    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384          
stack_top: