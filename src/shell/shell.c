#include "shell.h"
#include "../fs/fat.h"
#include "../fs/ata.h"
#include "../kernel/rtc.h"
#include "../lib/string.h"
#include "../kernel/terminal.h"
#include "vga.h"

// Ces symboles viennent du script de liaison (linker.ld)
extern uint32_t _kernel_start;
extern uint32_t _kernel_end;
extern uint32_t cursor_pos;
extern uint8_t cursor_x; 
extern struct fat_bpb bpb; // Indique au linker que bpb existe ailleurs (dans fat.c)

// On indique que la table et la fonction existent ailleurs
extern font_entry_t font_table[];
extern void switch_font(char *name);

uint32_t get_esp() {
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    return esp;
}

uint8_t prompt_len = 0; // Contiendra la position X juste après le "> "

extern volatile int command_ready;
extern char command_buffer[];
extern uint32_t buffer_idx;
extern uint32_t cursor_pos;
extern uint8_t prompt_len;

typedef void (*entry_point_t)(void);

uint32_t shell_stack_ptr = 0;

void run_application(char* filename) {
    // 1. Trouver le fichier sur le disque
    int cluster = fat_find_file_cluster(filename);
    if (cluster < 2) {
        kprintf("Erreur : Application '%s' non trouvée.\n", filename);
        return;
    }

    // 2. Adresse de destination (5 Mo)
    uint8_t* load_address = (uint8_t*)0x500000;
    kprintf("Chargement de %s à 0x%x...\n", filename, load_address);

    // 3. Lire le fichier cluster par cluster dans la RAM
    uint16_t current_cluster = (uint16_t)cluster;
    uint8_t* target = load_address;

    while (current_cluster >= 2 && current_cluster < 0xFFF8) {
        // Calcul du LBA
        uint32_t lba = fat_cluster_to_lba(current_cluster);
        
        // Lecture du secteur (512 octets)
        ata_read_sector(lba, (uint16_t*)target);
        
        target += 512;
        current_cluster = fat_get_next_cluster(current_cluster);
    }

    // 4. SAUT VERS L'APPLICATION
    kprintf("Lancement...\n");
    entry_point_t start_app = (entry_point_t)load_address;
    
    // /!\ ATTENTION : Ici le noyau donne le contrôle total à l'appli
    start_app(); 
    
    kprintf("\n[Retour au Noyau]\n");
}

void command_info() {
    char hex_buffer[12];
    
    kprintf("--- %s System Info ---\n",OS_NAME);

    // Adresses (en Hexa)
    itoa_hex(hex_buffer, (uint32_t)&_kernel_start);
    kprintf("Kernel Start : "); kprintf(hex_buffer); kprintf("\n");

    itoa_hex(hex_buffer, (uint32_t)&_kernel_end);
    kprintf("Kernel End   : "); kprintf(hex_buffer); kprintf("\n");

    // Taille (en Décimal)
    uint32_t kernel_size = (uint32_t)&_kernel_end - (uint32_t)&_kernel_start;
    itoa(hex_buffer, kernel_size);
    kprintf("Kernel Size  : "); kprintf(hex_buffer); kprintf(" bytes\n");

    kprintf("------------------------------\n");
}

void interpret_command(char* buffer) {
    trim(buffer);
    if (strlen(buffer) == 0) return;

    // --- GESTION DU MULTI-COMMANDES (;) ---
    char *next_cmd = strchr(buffer, ';');
    if (next_cmd) {
        *next_cmd = '\0';        // On coupe la chaîne au point-virgule
        next_cmd++;              // On pointe sur le début de la commande suivante
        
        // On exécute la première partie (ex: touch test)
        process_single_command(buffer);
        
        // On exécute la suite récursivement (ex: ls)
        interpret_command(next_cmd);
        return;
    }

    // Si pas de point-virgule, on exécute normalement
    process_single_command(buffer);
}

void process_single_command(char* buffer) {
    trim(buffer); // Nettoie le \n et les espaces inutiles
    if (strlen(buffer) == 0) return;

    // --- AIDE ---
    if (strcmp(buffer, "help") == 0) {
        kprintf("\n%v--- Commandes %s ---%v\n", VGA_CYAN,OS_NAME, VGA_WHITE);
        kprintf("ls, touch, cat, echo, rm, clear, free, malloc, reboot, ver\n");
    }
    // --- SYSTEME DE FICHIERS ---
    else if (strcmp(buffer, "ls") == 0) {
         fat_ls();
    }
    else if (str_starts_with(buffer, "touch ")) {
        char* filename = buffer + 6;
        trim(filename);
        if (filename[0] != '\0') {
            // fat_create_file(filename);
            fat_touch(filename);
        } else {
            kprintf("Usage: touch <filename>\n");
        }
    }
    else if (str_starts_with(buffer, "cat ")) {
        char *raw_name = buffer + 3; // On pointe juste après "cat"
        
        // On saute tous les espaces restants
        while (*raw_name == ' ') raw_name++;

        if (*raw_name == '\0') {
            kprintf("Usage: cat <filename>\n");
        } else {
            // On nettoie la fin (enlever le \n ou les espaces de fin)
            trim(raw_name);
            fat_cat(raw_name);
        }
    }
    else if (memcmp(buffer, "more ", 5) == 0) {
        // On récupère le nom du fichier (ce qui suit "more ")
        //shell_more(buffer + 5);
        fat_cat(buffer+5);
    }
    else if (str_starts_with(buffer, "echo ")) {
        // Logique simplifiée du echo >
        char *text = buffer + 5;
        char *chevron = strchr(text, '>');
        if (chevron) {
            *chevron = '\0';
            char *filename = chevron + 1;
            trim(text); trim(filename);
            write_file(filename, text);
        } else kprintf("\n%s\n", text);
    }
    else if (str_starts_with(buffer, "cp ")) {
        char* args = buffer + 3;
        char src[13], dest[13];
        int i = 0, j = 0;

        // Extraire le premier nom (source)
        while (args[i] != ' ' && args[i] != '\0' && i < 12) {
            src[i] = args[i];
            i++;
        }
        src[i] = '\0';

        if (args[i] == ' ') {
            i++; // Passer l'espace
            // Extraire le deuxième nom (destination)
            while (args[i] != ' ' && args[i] != '\0' && j < 12) {
                dest[j] = args[i];
                i++; j++;
            }
            dest[j] = '\0';

            if (dest[0] != '\0') {
                fat_copy_file(src, dest);
            } else {
                kprintf("Usage: cp <src> <dest>\n");
            }
        }
    }
    else if (str_starts_with(buffer, "rm ")) {
        char* filename = buffer + 3;
        trim(filename);
        if (filename[0] != '\0') {
            fat_remove_file(filename);
        } else {
            kprintf("Usage: rm <filename>\n");
        }
    }
    else if (str_starts_with(buffer, "edit ")) {
        // On saute "edit " (5 caractères)
        char* args = buffer + 5;
        
        // Séparation rudimentaire : on cherche le premier espace après le nom du fichier
        char filename[13];
        int i = 0;
        while(args[i] != ' ' && args[i] != '\0' && i < 12) {
            filename[i] = args[i];
            i++;
        }
        filename[i] = '\0';

        if (args[i] == ' ') {
            char* text = args + i + 1; // Le texte commence après l'espace
            //fat_overwrite_file_content(filename, text);

            fat_overwrite_file(filename, text);
        } else {
            kprintf("Usage: edit <file> <text>\n");
        }
    }

    // --- GESTION DES REPERTOIRES ---
    else if (str_starts_with(buffer, "mkdir ")) {
        char* dirname = buffer + 6;
        trim(dirname);
        if (strlen(dirname) > 0) {
            fat_mkdir(dirname);
        } else {
            kprintf("Usage: mkdir <dirname>\n");
        }
    }
    else if (str_starts_with(buffer, "cd ")) {
        char* dirname = buffer + 3;
        trim(dirname);
        
        if (strlen(dirname) > 0) {
            // On utilise la fonction spécialisée fat_cd
            if (fat_cd(dirname) == 0) {
                // Optionnel : afficher un message ou laisser le prompt faire le travail
            }
        } else {
            kprintf("Usage: cd <dirname>\n");
        }
    }
    // --- MEMOIRE & SYSTEME ---
    else if (strcmp(buffer, "free") == 0 || strcmp(buffer, "malloc") == 0) {
        kprintf("\n[MEM] Heap check...\n");
        // Appel de tes fonctions de debug mémoire ici
    }
    else if (strcmp(buffer, "clear") == 0) {
        clear_screen();
    }
    else if (strcmp(buffer, "reboot") == 0) {
        reboot();
    }
    else if (strcmp(buffer, "ver") == 0) {
        kprintf("\n%s v%s\n", OS_NAME, OS_VERSION);
    }
    else if (strcmp(buffer, "info") == 0){
        command_info();
    }
    // --- DIVERS ---
    else if (strcmp(buffer, "date") == 0) {
        int day, month, year, hour, min, sec;
        
        // On appelle ton nouveau driver
        get_current_datetime(&day, &month, &year, &hour, &min, &sec);
        
        // Affichage formaté : DD/MM/YYYY HH:MM:SS
        kprintf("Date actuelle : %d/%d/%d %d:%d:%d\n", day, month, year, hour, min, sec);
    }
    else if (str_starts_with(buffer, "tz ")) { // pouvoir changer d'heure sans redémarrer
        int new_tz = buffer[3] - '0';
        if (new_tz >= -12 && new_tz <= 12) {
            timezone_offset = new_tz;
            kprintf("Fuseau regle sur GMT%+d\n", timezone_offset);
        }
    }
    else if (str_starts_with(buffer, "set_tz ")) {
        char new_val[5];
        new_val[0] = 'T'; new_val[1] = 'Z'; new_val[2] = '=';
        new_val[3] = buffer[7]; // Le chiffre tapé
        new_val[4] = '\0';
        fat_overwrite_file_content("CONFIG.CNF", new_val);
        kprintf("Fuseau mis a jour. Redemarrez pour appliquer.\n");
    }
    else if (strcmp(buffer,"fonttest")==0){
        kprintf("\n--- Test Atlas Police ---\n");
        for (int i = 0; i < 256; i++) {
            putc((char)i);
            if (i % 32 == 31) putc('\n'); // Retour à la ligne toutes les 32 lettres
        }
        kprintf("\n-------------------------\n");
    }
    else if (memcmp(command_buffer, "setfont ", 8) == 0) {
        char *font_name = command_buffer + 8;
        // On enlève l'éventuel \n à la fin
        for(int i=0; font_name[i]; i++) if(font_name[i]=='\n') font_name[i]=0;
        
        switch_font(font_name);
    }
    else if (strcmp(command_buffer, "listfonts") == 0) {
        kprintf("Polices disponibles :\n");
        for (int i = 0; font_table[i].name != 0; i++) {
            kprintf(" - %s\n", font_table[i].name);
        }
    }
    // --- CMD ---
    else if (str_starts_with(buffer, "run ")) {
        char* cmd = buffer + 4;
        //char filename[13];
        char* arg = strchr(cmd, ' '); // Cherche un espace après le nom du fichier

        if (arg) {
            *arg = '\0'; // Coupe la chaîne pour isoler le nom du fichier
            arg++;       // Pointeur vers le début de l'argument
        }

        if (fat_read_file(cmd, (uint8_t*)0x200000)) {
            kprintf("Chargement fini.\n"); 
            
            // On s'assure que les interruptions sont activées pour l'appli
            asm volatile("sti"); 
            
            // Passage de l'argument en EDX
            asm volatile("mov %0, %%edx" : : "r"(arg));
            
            void (*entry)() = (void(*)())0x200000;
            entry();

            // IMPORTANT : Après le retour de l'appli, on peut vider EDX 
            // pour éviter que la prochaine commande ne croie recevoir un argument
            asm volatile("xor %%edx, %%edx" ::: "edx");
        }
    }
    else {
        kprintf("\nCommande '%s' inconnue.\n", buffer);
    }
}

/*
Comme tu es à l'intérieur d'une IRQ, la pile contient :

    Les registres de l'application (sauvegardés par irq1_wrapper).

    L'adresse de retour vers l'application (iret).

Si tu veux arrêter l'appli, il faut "écraser" tout ça et recharger la pile du Shell.
*/
void force_exit() {
    // 1. On réinitialise l'état du clavier pour éviter que le Shell 
    // n'interprète des résidus de l'application.
    command_ready = 0;
    buffer_idx = 0;
    cursor_pos = 0;
    memset(command_buffer, 0, BUFFER_SIZE);

    // 2. Le saut en assembleur
    // On restaure l'ESP sauvegardé, on réactive les interruptions (STI)
    // Et on saute au début de la boucle.
    asm volatile(
        "mov %0, %%esp\n"         // Restaure la pile du noyau (Shell)
        "sti\n"                   // S'assure que les interruptions sont ON
        "push %1\n"               // Pousse l'adresse de destination
        "ret\n"                   // "Retourne" vers shell_loop
        : 
        : "m"(shell_stack_ptr), "r"(shell_loop)
        : "memory"
    );
}

/*void shell_loop() {
    char buffer[256];
    while (1) {
        if (current_dir_cluster == 0) {
            kprintf("\n%v%s / > %v", VGA_CYAN,OS_NAME, VGA_WHITE);
        } else {
            // On affiche le numéro de cluster pour debugger plus facilement
            kprintf("\n%v%s [C:%d] > %v", VGA_LIGHT_GREEN, OS_NAME,current_dir_cluster, VGA_WHITE);
        }
        
        readline(buffer, 256);
        interpret_command(buffer);
    }
}*/

/*void shell_loop() {
    while (1) {
        // 1. Afficher le prompt
        if (current_dir_cluster == 0) {
            kprintf("\n%v%s / > %v", VGA_CYAN, OS_NAME, VGA_WHITE);
        } else {
            kprintf("\n%v%s [C:%d] > %v", VGA_LIGHT_GREEN, OS_NAME, current_dir_cluster, VGA_WHITE);
        }
        
        prompt_len = cursor_x; // Mémorise la fin du prompt
        update_hardware_cursor();

        // 2. Attendre que l'interruption clavier lève le drapeau
        command_ready = 0;
        while (command_ready == 0) {
            asm volatile("hlt"); // Met le CPU en pause basse consommation en attendant une interruption
        }

        // 3. Exécuter si une commande est prête
        if (command_ready == 1) {
            interpret_command(command_buffer);
        }

        // 4. Reset du buffer pour la suite
        buffer_idx = 0;
        cursor_pos = 0;
        memset(command_buffer, 0, 256);
    }
}
*/

void shell_loop() {
    // On enregistre l'état de la pile juste ici pour ^C
    // Désormais, force_exit nous téléportera à cette ligne exacte.
    asm volatile("mov %%esp, %0" : "=m"(shell_stack_ptr));

    while (1) {
        kprintf("\n> ");
        prompt_len = cursor_x; 
        update_hardware_cursor();

        // Attente du signal clavier
        while (command_ready == 0) {
            asm volatile("hlt");
        }

        // --- DEBUT ZONE CRITIQUE ---
        asm volatile("cli");
        
        // On récupère la commande
        char work_buffer[BUFFER_SIZE];
        strcpy(work_buffer, command_buffer);

        // ON NETTOIE TOUT AVANT DE LANCER L'APPLI
        memset(command_buffer, 0, BUFFER_SIZE);
        buffer_idx = 0;
        cursor_pos = 0;
        command_ready = 0; // On baisse le drapeau
        
        asm volatile("sti");
        // --- FIN ZONE CRITIQUE ---

        kprintf("\n");
        interpret_command(work_buffer);
        
        // Sécurité supplémentaire : on vide à nouveau après le retour de l'appli
        command_ready = 0; 
    }
}

void shell_more(char* filename) {
    // 1. On cherche d'abord si le fichier existe pour connaître sa taille
    // On pourrait utiliser fat_find_file_cluster, mais on a besoin de la taille 
    // pour allouer juste ce qu'il faut.
    
    // Si tu n'as pas de fonction qui retourne la structure directory_entry facilement,
    // on va allouer un buffer généreux (ex: 32 Ko) ou lire cluster par cluster.
    
    uint8_t* buffer = (uint8_t*)kmalloc(32768); // Allocation de 32Ko
    if (!buffer) {
        kprintf("Erreur memoire\n");
        return;
    }

    // 2. Utilisation de TA fonction fat_read_file
    if (fat_read_file(filename, buffer)) {
        int lines = 0;
        
        for (int i = 0; buffer[i] != '\0'; i++) {
            putc(buffer[i]);

            if (buffer[i] == '\n') {
                lines++;
            }

            // Pause toutes les 24 lignes
            if (lines >= 24) {
                kprintf("\n%v-- PRESSER UNE TOUCHE POUR LA SUITE --%v", VGA_YELLOW, VGA_WHITE);
                
                // On utilise ta fonction keyboard_getc() qui est bloquante
                keyboard_getc(); 
                
                // On efface la ligne de prompt "PRESSER..."
                kprintf("\r                                         \r");
                lines = 0;
            }
        }
    } else {
        kprintf("Fichier '%s' introuvable.\n", filename);
    }

    kfree(buffer);
}



