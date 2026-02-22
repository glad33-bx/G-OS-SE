; Fonctions d'E/S (io.h / io.asm)
; Pour que inb et outb fonctionnent, on a besoin de ces fonctions en assembleur 
; car le C ne peut pas accéder directement aux ports matériels.

global inb
global outb

inb:
    mov edx, [esp + 4] ; Port
    in al, dx
    ret

outb:
    mov edx, [esp + 4] ; Port
    mov al, [esp + 8]  ; Valeur
    out dx, al
    ret