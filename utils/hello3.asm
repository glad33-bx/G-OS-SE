[BITS 32]
org 0x200000

start:
    ; Message 1 : Rouge (0x0C pour rouge clair)
    mov eax, 3          ; Syscall kprintf_color
    mov ebx, msg_red
    mov ecx, 0x0C       ; Rouge clair
    int 0x80

    ; Message 2 : Vert (0x0A pour vert clair)
    mov eax, 3
    mov ebx, msg_green
    mov ecx, 0x0A       ; Vert clair
    int 0x80

    ret

msg_red   db "Ce texte est en rouge via syscall.", 10, 0
msg_green db "Et celui-là est en vert !", 10, 0