#include "terminal.h"
#include "keyboard.h"
#include "../fs/fs.h"
#include "gos_memory.h"
#include "mem.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "../lib/string.h"
#include "../lib/gos_types.h"
#include "../lib/config.h"
#include "../shell/shell.h"
#include "ata.h"
#include "fat.h"
#include "rtc.h"
#include "vga.h"
#include "paging.h"

// Au démarrage, après avoir initialisé le système de fichiers, on lit le fichier de config.
/*
La structure du fichier config.cnf

Pour faire simple et efficace, nous allons créer un fichier texte très basique sur ton disque FAT.
    Contenu du fichier : TZ=1 (pour GMT+1) ou TZ=2 (pour l'été).
*/
void load_configuration() {
    // 1. On prépare le nom au format FAT (8 + 3)
    // "CONFIG.CNF" -> "CONFIG  CNF"
    //char fat_name[12]="CONFIG.CNF\0";
    // Format FAT strict : 8 caractères de nom + 3 d'extension
    // Pas de point, comblé par des espaces.
    const char* fat_name = "CONFIG  CNF";
    //to_fat_name("CONFIG.CNF", fat_name); 

    // 2. On cherche avec le nom formaté
    int cluster = fat_find_file_cluster(fat_name);
    
    if (cluster == -1) {
        // Test de secours : on essaie en dur si ta fonction to_fat_name n'est pas prête
        cluster = fat_find_file_cluster("CONFIG  CNF");
    }

    if (cluster != -1) {
        // Attention : fat_get_file_content doit allouer ou utiliser un buffer global
        // car un pointeur vers une variable locale disparaîtrait.
        char* content = (char*)fat_get_file_content(cluster);
        if (!content) return;

        //kprintf("[DEBUG] Contenu brut : %s\n", content);

        for (int i = 0; i < 508; i++) {
            if (content[i] == 'T' && content[i+1] == 'Z' && content[i+2] == '=') {
                timezone_offset = content[i+3] - '0';
                kprintf("[INIT] Fuseau horaire applique : GMT+%d\n", timezone_offset);
                return;
            }
            // --- Gestion Clavier (KBD=fr ou KBD=us) ---
            if (content[i] == 'K' && content[i+1] == 'B' && content[i+2] == 'D' && content[i+3] == '=') {
                char layout[3];
                layout[0] = content[i+4];
                layout[1] = content[i+5];
                layout[2] = '\0';
                keyboard_set_layout(layout);
            }
            // --- Gestion Police (FONT=terminus) ---
            if (content[i] == 'F' && content[i+1] == 'O' && content[i+2] == 'N' && content[i+3] == 'T' && content[i+4] == '=') {
                char font_name[16];
                int j = 0;
                // On récupère le nom jusqu'à un espace ou un retour ligne
                while (content[i+5+j] > 32 && j < 15) {
                    font_name[j] = content[i+5+j];
                    j++;
                }
                font_name[j] = '\0';
                switch_font(font_name);
                kprintf("[INIT] Police chargee : %s\n", font_name);
            }
        }
    } else {
        kprintf("[DEBUG] CONFIG.CNF introuvable (meme avec espaces)\n");
    }
}


/**
 * kernel_main - Point d'entrée principal de GillesOS
 * kernel.c est maintenant très propre et suit la structure standard d'un vrai noyau : 
 * Initialisation CPU → Mémoire → Système de fichiers → Interface.
 * 
 */
void kmain() {
    // 1. Setup initial
    init_terminal();

    load_vga_font();

    init_gdt(); // Global Descriptor Table
    init_idt(); // Interrupt Descriptor Table
    init_timer(100);

    //init_paging();

    // 2. Mémoire (Indispensable pour la suite)
    init_memory(); 

    // 3. Matériel "Lourd" (Disque)
    fat_init(); // On initialise le driver ATA et on lit le BPB

    // 4. Interface (Clavier + Shell)
    // 4.0 : Entrées Utilisateur
    kprintf("%v[KBD]%v Configuration du clavier... ", VGA_YELLOW, VGA_WHITE);
    init_keyboard(); 

    // 4.1 Affichage du logo
    kprintf("\n%v------------------------------------------------\n", VGA_CYAN);
    kprintf("%v  %s Kernel v%s %v- Boot Sequence Starting\n", VGA_LIGHT_CYAN, OS_NAME,OS_VERSION,VGA_WHITE);
    kprintf("%v------------------------------------------------\n\n", VGA_CYAN);

    load_configuration();

    // 6. Activation des interruptions
    // On ne fait ça QUE quand tout est prêt à répondre
    __asm__ __volatile__("sti");

    #if DEBUG
        for (int i = 0; i < 256; i++) {
            //kprintf("%d -",i);
            putc((unsigned char)i);
            if (i % 32 == 0) kprintf("\n");
        }
    #endif

    //kprintf("\n%s pret.\n> ", OS_NAME);
    shell_loop();

    while (1) {
        asm volatile("hlt");
    }
}