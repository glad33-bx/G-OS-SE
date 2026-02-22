[BITS 32]
[GLOBAL _start]
[EXTERN main]

_start:
    call main
    ; Si main revient, on force un exit(0) pour éviter le crash
    mov eax, 5      ; Syscall EXIT
    mov ebx, 0      ; Code retour 0
    int 0x80