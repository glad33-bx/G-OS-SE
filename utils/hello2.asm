[BITS 32]
org 0x200000

start:
    mov eax, 1          ; Fonction 1 : print
    mov ebx, msg        ; EBX = adresse du texte
    int 0x80            ; APPEL AU NOYAU

    mov eax, 1
    mov ebx, msg2
    int 0x80

    ret                 ; Retour au shell

msg  db "Execution via Syscall 0x80 reussie !", 10, 0
msg2 db "GillesOS est maintenant protege !", 10, 0