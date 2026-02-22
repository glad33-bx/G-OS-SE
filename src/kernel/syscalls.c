#include "syscalls.h"
#include "terminal.h"
#include "idt.h"
#include "../kernel/syscall_nums.h"
#include "keyboard.h"

/*
C'est ici que le Kernel va décider quoi faire selon la valeur du registre EAX.
*/

/*void syscall_handler(struct regs *r) {
    // Le numéro de l'appel système est dans EAX
    switch (r->eax) {
        case 1: // Syscall 1 : kprintf
            // On passe l'adresse de la chaîne dans EBX
            kprintf((char*)r->ebx);
            break;

        case 2: // Syscall 2 : clear_screen
            clear_screen();
            break;
        case 3: // Syscall 3 : kprintf_color
            // eax = 3, ebx = message, ecx = couleur
            kprintf_color((char*)r->ebx, (uint8_t)r->ecx);
            break;
        case 5: // Syscall EXIT
            kprintf("\n[Processus termine avec le code %d]\n", r->ebx);
            // Pour l'instant, on se contente de retourner, 
            // mais plus tard, c'est ici qu'on libérera la mémoire.
            break;
        default:
            kprintf("Syscall inconnu !");
            break;
    }
}*/

void syscall_handler(struct regs *r) {

     // Le numéro de l'appel système est dans EAX
    switch (r->eax) {
        case SYS_PRINT: // Syscall 1 : kprintf
            // On passe l'adresse de la chaîne dans EBX
            kprintf((char*)r->ebx);
            break;
        case SYS_CLEAR: // Syscall 2 : clear_screen
            clear_screen();
            break;
        case SYS_PRINT_COLOR: // Syscall 3 : kprintf_color
            // eax = 3, ebx = message, ecx = couleur
            kprintf_color((char*)r->ebx, (uint8_t)r->ecx);
            break;
        case SYS_PUTC: // Nouveau : Affichage d'un seul caractère
            putc((char)r->ebx);
            break;
        case SYS_EXIT: // Syscall EXIT
            kprintf("\n[Processus termine avec le code %d]\n", r->ebx);
            // Pour l'instant, on se contente de retourner, 
            // mais plus tard, c'est ici qu'on libérera la mémoire.
            break;
        case SYS_GETC:
            // On stocke le résultat dans EAX pour que l'appli le récupère
            r->eax = (uint32_t)keyboard_getc(); 
            break;
        default:
            kprintf("Syscall inconnu !");
            break;
    }
}