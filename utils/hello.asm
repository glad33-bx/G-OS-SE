[BITS 32]
org 0x200000

start:
    mov esi, msg        ; Pointeur sur le message
    mov edi, 0xB8000    ; Direction la mémoire vidéo (haut de l'écran)
    mov ah, 0x0E        ; Couleur jaune

.loop:
    lodsb               ; Charge le caractère dans AL et incrémente ESI
    test al, al         ; Est-ce la fin (0) ?
    jz .done
    stosw               ; Écrit AL et AH dans EDI (mémoire vidéo) et avance
    jmp .loop

.done:
    ret

msg db "Bravo Gilles ! L'application tourne !", 0