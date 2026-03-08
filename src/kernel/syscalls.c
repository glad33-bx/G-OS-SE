#include "syscalls.h"
#include "terminal.h"
#include "idt.h"
#include "fat.h"
#include "../kernel/syscall_nums.h"
#include "keyboard.h"
#include "../shell/shell.h"

// Variable globale pour stocker la commande tapée
char current_cmdline[128];


void set_current_command(const char* cmd) {
    if (cmd) {
        strncpy(current_cmdline, cmd, 127);
        current_cmdline[127] = '\0';
    }
}

/*
C'est ici que le Kernel va décider quoi faire selon la valeur du registre EAX.
*/
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
            #if DEBUG
                kprintf("\n[Processus termine avec le code %d]\n", r->ebx);
            #else
                kprintf("\nCode retour : %d\n", r->ebx);
            #endif
            // Pour l'instant, on se contente de retourner, 
            // mais plus tard, c'est ici qu'on libérera la mémoire.
            break;
        case SYS_GETC:
            // On stocke le résultat dans EAX pour que l'appli le récupère
            r->eax = (uint32_t)keyboard_getc(); 
            break;
        case SYS_GOTOXY: // eax = numéro choisi (ex: 7)
            // ebx = x, ecx = y
            update_cursor_position(r->ebx, r->ecx); 
            break;
        case SYS_READ_FILE: {
            char* fname = (char*)r->ebx;
            uint8_t* dest = (uint8_t*)r->ecx;
            uint32_t len = r->edx;

            struct fat_dir_entry entry;
            uint32_t cluster = fat_resolve_path(fname, &entry);

            if (cluster != 0xFFFFFFFF) {
                void* data = fat_get_file_content(cluster);
                
                // On ne copie pas plus que le fichier, ni plus que le buffer
                uint32_t size_to_copy = (entry.size < len) ? entry.size : len;
                
                memcpy(dest, data, size_to_copy);
                
                // On libère data ici si fat_get_file_content fait un malloc !
                // free(data); 

                r->eax = size_to_copy; // On renvoie la taille lue
            } else {
                r->eax = -1; // Convention standard : -1 pour erreur
            }
            break;
        }
        case SYS_GET_ARGV:
            // EBX contient l'adresse du buffer dans l'application
            strcpy((char*)r->ebx, global_cmd_buffer);
            break;
        // case SYS_LS:
        //     r->eax = fat_get_dir_list((char*)r->ebx, (char*)r->ecx);
        //     break;
        case SYS_LS: {
            char* path = (char*)r->ebx;
            char* user_buffer = (char*)r->ecx;
           
            int result = fat_list_to_buffer(path, user_buffer);
            break;
        }
        // case SYS_SET_COLOR:
        //     set_color((uint8_t)r->ebx); // Appelle ta fonction existante de terminal
        //     break;
        case SYS_SET_COLOR:
            // r->ebx contient la couleur de texte (fg)
            // r->ecx contient la couleur de fond (bg)
            set_color((uint8_t)r->ebx, (uint8_t)r->ecx); 
            break;
        default:
            kprintf("Syscall inconnu !");
            break;
    }
}