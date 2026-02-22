; src/interrupt.asm
; "Pont" Assembleur
; Le processeur ne peut pas appeler directement une fonction C lors d'une interruption 
; car il doit sauvegarder les registres. On utilise souvent un "wrapper" en assembleur :

[BITS 32]
extern keyboard_handler
extern timer_handler

; On rend ces labels visibles pour le Linker
global keyboard_handler_asm
global irq0_wrapper
global irq1_wrapper

keyboard_handler_asm:
irq1_wrapper:
    pusha          ; Sauve les registres
    call keyboard_handler

    ; Signal de fin d'interruption au PIC (Master)
    mov al, 0x20
    out 0x20, al

    popa           ; Restaure les registres
    iret           ; INTERRUPT RETURN (essentiel !)

irq0_wrapper:
    pusha
    call timer_handler

    ; Signal de fin d'interruption au PIC (Master)
    mov al, 0x20
    out 0x20, al

    popa
    iret

[EXTERN syscall_handler]
[GLOBAL syscall_wrapper]

syscall_wrapper:
    push 0          ; Error code fictif
    push 0x80       ; Numéro d'interruption
    pusha           ; Sauve EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10    ; Data segment du noyau
    mov ds, ax
    mov es, ax

    push esp        ; Donne le pointeur de la structure regs au C
    call syscall_handler
    add esp, 4      ; Nettoie l'argument esp

    pop gs
    pop fs
    pop es
    pop ds
    popa            ; Restaure les registres généraux
    add esp, 8      ; Nettoie le numéro d'int et le code d'erreur
    iretd           ; Retourne à l'application
