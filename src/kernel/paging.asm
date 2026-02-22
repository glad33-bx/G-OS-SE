[bits 32]

; Le C ne peut pas toucher aux registres de contrôle du processeur. 
; Il nous faut une petite fonction en ASM pour charger notre annuaire
; dans CR3 et basculer l'interrupteur dans CR0.

global load_page_directory
global enable_paging

section .text

load_page_directory:
    mov eax, [esp + 4]  ; Récupère l'argument (adresse du répertoire)
    mov cr3, eax        ; Charge l'adresse dans CR3
    ret

enable_paging:
    mov eax, cr0
    or eax, 0x80000000  ; Active le bit de pagination (PG)
    mov cr0, eax
    ret