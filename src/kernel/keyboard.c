#include "../lib/config.h"
#include "keyboard.h"
#include "vga.h"
#include "pit.h"

#define BUFFER_SIZE 256

// --- Variables d'état privées ---
char command_buffer[BUFFER_SIZE];
uint32_t buffer_idx = 0; // Longueur totale de la commande
uint32_t cursor_pos = 0; // Position actuelle du curseur dans le buffer

int is_extended = 0;

// Système d'historique simple
static char history[HISTORY_SIZE][256];
static int history_count = 0;
static int history_index = -1; // -1 = commande en cours

int shift_pressed = 0;
int ctrl_pressed = 0;
int caps_lock = 0;
int extended_mode = 0;
static char dead_key = 0; // 0 = rien, '^' = circonflexe, '\"' = tréma
static int altgr_pressed = 0;
static int win_pressed = 0;
static int menu_pressed=0;
//static int fn_pressed=0;

// --- Tables AZERTY ---
// Extrait de ton driver de clavier (keyboard.c)
// Index 0x02 à 0x0B correspond à la ligne 1234567890
// Table AZERTY pour GillesOS (CP437/VGA compatible)
/*
Correspondances pour ton tableau :
    é = 0x82
    è = 0x8A
    ç = 0x87
    à = 0x85
    ù = 0x97
*/

/*
En mode ffreestanding, ton écran n'affiche pas de l'UTF-8, 
mais ce qu'il y a dans la mémoire vidéo 0xB8000. 
Cette mémoire utilise la table IBM PC (CP437). Voici la correspondance pour tes touches :
° (Degré) : 0xF8
£ (Livre) : 0x9C
µ (Micro) : 0xE6
§ (Paragraphe) : 0x15
¨ (Tréma) : Le tréma est souvent une "touche morte", mais tu peux utiliser 0xF9 (un point médian haut) ou 0x9D comme substitut.
*/

// kbd_map_french corrigé pour compatibilité maximale (ASCI 7-bits)
unsigned char kbd_map_french[128] = {
    [0x01] = 27, 
    [0x02] = '&',  [0x03] = 0x82, [0x04] = '"',  [0x05] = '\'', [0x06] = '(', 
    [0x07] = '-',  [0x08] = 0x8A, [0x09] = '_',  [0x0A] = 0x87, [0x0B] = 0x85, 
    [0x0C] = ')',  [0x0D] = '=',  [0x0E] = '\b',
    [0x0F] = '\t', [0x10] = 'a',  [0x11] = 'z',  [0x12] = 'e',  [0x13] = 'r', 
    [0x14] = 't',  [0x15] = 'y',  [0x16] = 'u',  [0x17] = 'i',  [0x18] = 'o', 
    [0x19] = 'p',  [0x1A] = '^',  [0x1B] = '$',  [0x1C] = '\n',
    [0x1E] = 'q',  [0x1F] = 's',  [0x20] = 'd',  [0x21] = 'f',  [0x22] = 'g', 
    [0x23] = 'h',  [0x24] = 'j',  [0x25] = 'k',  [0x26] = 'l',  [0x27] = 'm', 
    [0x28] = 0x97, // ù (Code CP437)
    [0x29] = 0xFD, // ² (Le petit 2 au bon indice 0x29)
    [0x2B] = '*',  
    [0x2C] = 'w',  [0x2D] = 'x',  [0x2E] = 'c',  [0x2F] = 'v',  [0x30] = 'b', 
    [0x31] = 'n',  [0x32] = ',',  [0x33] = ';',  [0x34] = ':',  [0x35] = '!',
    [0x39] = ' ',  [0x56] = '<'
};

// kbd_map_french_shift corrigé
unsigned char kbd_map_french_shift[128] = {
    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',  [0x05] = '4',  [0x06] = '5', 
    [0x07] = '6',  [0x08] = '7',  [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0', 
    [0x0C] = 0xF8, // ° (degré CP437)
    [0x0D] = '+',  [0x0E] = '\b',
    [0x10] = 'A',  [0x11] = 'Z',  [0x12] = 'E',  [0x13] = 'R',  [0x14] = 'T', 
    [0x15] = 'Y',  [0x16] = 'U',  [0x17] = 'I',  [0x18] = 'O',  [0x19] = 'P', 
    [0x1A] = 0xA8, // ¨ (Tréma)
    [0x1B] = 0x9C, // £ (Livre CP437)
    [0x1E] = 'Q',  [0x1F] = 'S',  [0x20] = 'D',  [0x21] = 'F',  [0x22] = 'G', 
    [0x23] = 'H',  [0x24] = 'J',  [0x25] = 'K',  [0x26] = 'L',  [0x27] = 'M', 
    [0x28] = '%',  [0x2B] = 0xE6, // µ
    [0x2C] = 'W',  [0x2D] = 'X',  [0x2E] = 'C',  [0x2F] = 'V',  [0x30] = 'B', 
    [0x31] = 'N',  [0x32] = '?',  [0x33] = '.',  [0x34] = '/',  [0x35] = 0x15, // § (Code CP437 standard)
    [0x56] = '>'
};

// --- MODE ALTGR (Symboles de programmation) ---
unsigned char kbd_map_french_altgr[128] = {
    [0x12] = 0xEE, // Touche E -> Symbole Euro (souvent 0xEE en PSF)
    [0x03] = '~',  // Touche 2 -> Tilde
    [0x04] = '#',  // Touche 3 -> Dièse
    [0x05] = '{',  // Touche 4 -> Accolade
    [0x06] = '[',  // Touche 5 -> Crochet
    [0x07] = '|',  // Touche 6 -> Pipe
    [0x08] = '`',  // Touche 7 -> Backtick
    [0x09] = '\\', // Touche 8 -> Backslash
    [0x0A] = '^',  // Touche 9 -> Circonflexe (parfois utilisé ici)
    [0x0B] = '@',  // Touche 0 -> Arobase
    [0x0C] = ']',  // Touche ) -> Crochet fermant
    [0x0D] = '}',  // Touche = -> Accolade fermante
};


// Table QWERTY US - Mode Normal
unsigned char kbd_map_us[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };

// Table QWERTY US - Mode Shift
unsigned char kbd_map_us_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'A', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };

extern uint8_t prompt_len; // On récupère la valeur définie dans shell.c

volatile char last_char = 0;

// variable globale pour signaler que la commande est prête
volatile int command_ready = 0; // volatile car modifiée dans une interruption

// Le pointeur "actif" que le handler utilisera
unsigned char* current_layout_normal = kbd_map_french;
unsigned char* current_layout_shift = kbd_map_french_shift;

void init_keyboard(){
    vga_set_euro_glyph(); // <--- cause un vidage écran si ligne 25

    // 1. Remapper le PIC (Donne de nouveaux numéros aux IRQ)
    outb(0x20, 0x11); // Initialisation du PIC Maître
    outb(0xA0, 0x11); // Initialisation du PIC Esclave
    outb(0x21, 0x20); // IRQ 0-7 deviennent les interruptions 0x20-0x27 (32-39)
    outb(0xA1, 0x28); // IRQ 8-15 deviennent les interruptions 0x28-0x2F
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0); // Masque : 0 = activer tout
    outb(0xA1, 0x0);

    // 2. Enregistrer le handler (IRQ 1 est maintenant 0x21 car 0x20 + 1)
    extern void irq1_wrapper();
    idt_set_gate(0x21, (uint32_t)irq1_wrapper, 0x08, 0x8E);

    // 3. Charger l'IDT (C'est ici que le registre IDT de QEMU doit se remplir)
    __asm__ __volatile__("lidt (%0)" : : "r"(&idtp));

    // 4. Activer les interruptions
    __asm__ __volatile__("sti");
}

// Fonction pour changer de langue
void keyboard_set_layout(char* lang){
    if (strcmp(lang, "us") == 0){
        current_layout_normal = kbd_map_us;
        current_layout_shift = kbd_map_us_shift;
        kprintf("[KBD] Layout set to US\n");
    }
    else{
        // Par défaut ou si "fr"
        current_layout_normal = kbd_map_french;
        current_layout_shift = kbd_map_french_shift;
        kprintf("[KBD] Layout set to FR\n");
    }
}

// fonction utilitaire pour transformer le couple (accent, lettre)
// en un caractère spécial (compatible avec la table VGA CP437)
char merge_accent(char accent, char letter) {
    if (accent == '^') { // Circonflexe
        switch (letter) {
            case 'a': return 0x83; // â
            case 'e': return 0x88; // ê
            case 'i': return 0x8C; // î
            case 'o': return 0x94; // ô
            case 'u': return 0x96; // û
            case 'J': return 0x1B ; //Ĵ
            case 'w': return 0x9E; // ẅ
            case 'Y': return 0xA9; // Â
        }
    }
    if (accent == '\"') { // Tréma
        switch (letter) {
            case 'a': return 0x84; // ä
            case 'e': return 0x89; // ë
            case 'i': return 0x8B; // ï
            case 'o': return 0x94; // ö (Attention: en CP437 ö et ô partagent souvent des codes proches)
            case 'u': return 0x81; // ü
            case 'y':return 0x98; // ÿ
            case 'A': return 0x8E; // Ä
            case 'O': return 0x99; // Ö
            case 'U': return 0x9A; // Ü
            case 'w': return 0xB8; // ẅ
        }
    }
    if (accent == '~') {
        switch (letter) {
            case 'n': return 0xA4; // ñ
            case 'N': return 0xA5; // Ñ
        }
    }
    if (accent == '`') { // Accent Grave
        switch (letter) {
            case 'a': return 0x85; // à
            case 'e': return 0x8A; // è
            case 'u': return 0x97; // ù
            case 'i': return 0x8D; // ì (si dispo)
            case 'o': return 0x95; // ò (si dispo)
            case 'w': return 0xB6; // `w 
        }
    }
    
    if (accent == (char)0xF8) { // Touche morte Degré (°)
        switch (letter) {
            case 'a': return 0x86; // å
            case 'A': return 0x8F; // Å
            case ' ': return 0xF8; // affiche juste le ° si espace
        }
    }
    return letter; 
}

/*char getc() {
    unsigned char scancode;
    // On attend que le bit 0 du port de statut (0x64) devienne 1
    while (!(inb(0x64) & 1)); 
    
    // On lit le scancode sur le port 0x60
    scancode = inb(0x60);

    // Si le bit 7 est mis, c'est que la touche est relâchée (on ignore)
    if (scancode & 0x80) return 0;

    return kbd_map_french[scancode];
}*/

// Cette fonction sera appelée par le syscall
char keyboard_getc(){
    // On s'assure que les interruptions sont actives
    // sinon le handler ne pourra jamais remplir last_char
    __asm__ __volatile__("sti");

    while (last_char == 0)
    {
        // Le processeur attend ici. Si une touche est pressée,
        // l'interruption clavier (IRQ1) va "réveiller" le CPU,
        // exécuter le handler, et modifier last_char.
        __asm__ __volatile__("hlt");
    }

    char c = last_char;
    last_char = 0;
    return c;
}

// --- Fonctions utilitaires d'affichage ---
/*
    Fonction pour charger une ancienne commande.
    Elle doit effacer la ligne actuelle et copier la commande de l'historique dans le buffer.
*/
void load_history(int direction)
{
    if (history_count == 0)
        return;

    // Calcul du nouvel index
    int new_index = history_index + direction;
    if (new_index < 0 || new_index >= history_count)
        return;

    history_index = new_index;

    // 1. Effacer visuellement la ligne actuelle
    while (buffer_idx > 0)
    {
        terminal_backspace();
        buffer_idx--;
    }

    // 2. Charger la commande
    strcpy(command_buffer, history[history_index]);
    buffer_idx = strlen(command_buffer);
    cursor_pos = buffer_idx;

    // 3. Afficher la nouvelle commande
    kprintf("%s", command_buffer);
}

/**
 * Aligne le curseur matériel (écran) sur notre position logique (buffer)
 */
void sync_cursor_visual() {
    // On s'assure de ne jamais revenir avant le prompt
    cursor_x = prompt_len + cursor_pos;
    update_hardware_cursor();
}

/**
 * Insère un caractère à la position actuelle (gère l'insertion au milieu)
 */

void insert_char(char c) {
    if (buffer_idx < BUFFER_SIZE - 1) {
        command_buffer[buffer_idx] = c;
        buffer_idx++;
        putc(c); 
        cursor_pos = buffer_idx;
        sync_cursor_visual();
    }
}
/**
 * Efface le caractère à GAUCHE du curseur (Backspace)
 */
void handle_backspace()
{
    if (cursor_pos > 0)
    {
        for (uint32_t i = cursor_pos - 1; i < buffer_idx; i++)
        {
            command_buffer[i] = command_buffer[i + 1];
        }
        buffer_idx--;
        cursor_pos--;

        sync_cursor_visual();
        uint16_t saved_x = cursor_x;

        // Réimprime la fin de ligne + un espace pour nettoyer le dernier char
        kprintf("%s ", &command_buffer[cursor_pos]);

        cursor_x = saved_x;
        update_hardware_cursor();
    }
}

/**
 * Efface le caractère SOUS le curseur (Suppr/Delete)
 */
void handle_delete()
{
    if (cursor_pos < buffer_idx)
    {
        for (uint32_t i = cursor_pos; i < buffer_idx; i++)
        {
            command_buffer[i] = command_buffer[i + 1];
        }
        buffer_idx--;

        uint16_t saved_x = cursor_x;
        kprintf("%s ", &command_buffer[cursor_pos]);

        cursor_x = saved_x;
        update_hardware_cursor();
    }
}

/**
 * Gère les touches après un 0xE0 (Flèches, Home, End)
 */
void handle_extended_keys(uint8_t scancode)
{
    switch (scancode)
    {
        case 0x4B: // Gauche
            if (cursor_pos > 0)
            {
                cursor_pos--;
                sync_cursor_visual();
            }
            break;
        case 0x4D: // Droite
            if (cursor_pos < buffer_idx)
            {
                cursor_pos++;
                sync_cursor_visual();
            }
            break;
        case 0x48: // Flèche HAUT (Historique)
            load_history(-1);
            return;
        case 0x50: // Flèche BAS (Historique)
            load_history(1);
            return;
        case 0x53: // Suppr
            handle_delete();
            break;
        case 0x47: // Home
            cursor_pos = 0;
            sync_cursor_visual();
            break;
        case 0x4F: // End
            cursor_pos = buffer_idx;
            sync_cursor_visual();
            break;
    }
}

// --- Handler Principal ---
void keyboard_handler(){
    uint8_t scancode = inb(0x60);

    // 1. Détection du mode étendu
    if (scancode == 0xE0){
        extended_mode = 1;
        outb(0x20, 0x20);
        return;
    }

    // 2. Gestion des touches relâchées (Break Codes)
    // Si le bit 7 est à 1, c'est une touche RELÂCHÉE
    if (scancode & 0x80){
        // Gérer le relâchement des modificateurs (Shift, Ctrl)
        uint8_t release_code = scancode & 0x7F;
        if (extended_mode){
            if (release_code == 0x38)
                altgr_pressed = 0;
            extended_mode = 0;
            win_pressed=0;
            menu_pressed=0;
        }
        else{
            if (release_code == 0x2A || release_code == 0x36)
                shift_pressed = 0;
            if (release_code == 0x1D)
                ctrl_pressed = 0;
            extended_mode = 0;
        }
        outb(0x20, 0x20); // ACK PIC
        return;           // ON S'ARRÊTE LÀ, on ne traite pas le reste pour un relâchement
    }

    // 3. Traitement étendu (Flèches)
    if (extended_mode){
        handle_extended_keys(scancode);
        extended_mode = 0;
        outb(0x20, 0x20);
        return;
    }

    // 4. Traitement standard
    switch (scancode){
        case 0x2A:
        case 0x36:
            shift_pressed = 1;
            break;
        case 0x1D:
            ctrl_pressed = 1;
            break;
        case 0x3A:
            caps_lock = !caps_lock;
            break;
        case 0x48: // Flèche Haut
            load_history(1);
            break;
        case 0x50: // Flèche Bas
            load_history(-1);
            break;
        case 0x0F: // Touche TAB
        {
            // à faire
        }
        default:{
            int use_shift = (caps_lock) ? !shift_pressed : shift_pressed;
            //char key = use_shift ? kbd_map_french_shift[scancode] : kbd_map_french[scancode];
            char key = 0;
           // 2. Priorité absolue aux tables (Ordre : AltGr > Shift > Normal)
            if (altgr_pressed) {
                // On vérifie que le scancode est dans la plage du tableau AltGr
                if (scancode < 128) key = kbd_map_french_altgr[scancode];
            } else if (use_shift) {
                key = current_layout_shift[scancode];
            } else {
                key = current_layout_normal[scancode];
            }

            // --- GESTION DES TOUCHES SPÉCIALES ---
            // CTRL-C ou ECHAP
            if ((ctrl_pressed && key == 'c') || scancode == 0x01) {
                kprintf("\n^C\n");
                outb(0x20, 0x20);
                force_exit();
                return;
            }
            // Exemple : CTRL + L pour effacer l'écran
            if (ctrl_pressed && key == 'l') {
                clear_screen();
                buffer_idx = 0; // On vide le buffer pour éviter d'exécuter "l"
                kprintf("> ");
                return;
            }
            // Ctrl+U : Très pratique pour effacer toute la ligne actuelle si on s'est trompé.
            if (ctrl_pressed && key == 'u') {
                while (buffer_idx > 0) {
                    terminal_backspace();
                    buffer_idx--;
                }
                return;
            }
            // Touches mortes (^ et ¨)
            // --- GESTION DES TOUCHES MORTES (Indice 0x1A) ---
            if (scancode == 0x1A) {
                // Si Shift est pressé, c'est un tréma, sinon c'est un circonflexe
                dead_key = (shift_pressed) ? '"' : '^';
                outb(0x20, 0x20);
                return;
            }
            // Détection du Tilde (AltGr + 2)
            if (altgr_pressed && scancode == 0x03) {
                dead_key = '~';
                outb(0x20, 0x20);
                return;
            }
            // ` 
            // Détection de l'Accent Grave (AltGr + 7)
            if (altgr_pressed && scancode == 0x08) {
                dead_key = '`';
                outb(0x20, 0x20);
                return;
            }
            // €
            if (altgr_pressed && scancode == 0x12) { // Touche E
                key = (unsigned char)0xEE; 
                // Laisse le code continuer vers l'affichage normal
            }
            // Détection du degré / cercle (Shift + ) ) -> Touche 0x0C
            if (shift_pressed && scancode == 0x0C) {
                dead_key = (char)0xF8; // On utilise le code degré comme touche morte
                outb(0x20, 0x20);
                return;
            }            
            if (key == '\b') {
                handle_backspace(); // Gère l'effacement visuel ET logique
            }
            else if (key == '\n') {
              /*command_buffer[buffer_idx] = '\0';
                command_ready = 1;*/
                command_buffer[buffer_idx] = '\0'; // On ferme la chaîne
                
                if (buffer_idx > 0) {
                    // On décale l'historique vers le bas
                    for (int i = HISTORY_SIZE - 1; i > 0; i--) {
                        strcpy(history[i], history[i - 1]);
                    }
                    strcpy(history[0], command_buffer);
                    if (history_count < HISTORY_SIZE) history_count++;
                }

                history_index = -1; // On revient à la ligne vide
                command_ready = 1;  // On libère le Shell
                putc('\n');         // On va à la ligne visuellement
                // ON NE VIDE PAS LE BUFFER ICI, c'est le Shell qui le fera après lecture.
            }
            else if (key != 0 && buffer_idx < BUFFER_SIZE - 1) {
/*                command_buffer[buffer_idx++] = key;
                command_buffer[buffer_idx] = '\0';
                cursor_pos = buffer_idx; // IMPORTANT pour sync_cursor_visual
                putc(key); 
                update_hardware_cursor();*/
                if (buffer_idx < BUFFER_SIZE - 1) {
                    // Fusion accent + lettre et insertion normale
                    if (dead_key != 0) {
                        key = merge_accent(dead_key, key);
                        dead_key = 0;
                    }
                    command_buffer[buffer_idx++] = key;
                    command_buffer[buffer_idx] = '\0';
                    cursor_pos = buffer_idx;
                    last_char = key;
                    putc(key);
                    update_hardware_cursor();
                }
                else { // Le buffer est plein
                    beep_async(50); // On lance le son et on quitte le handler direct !
                    return;
                }
            }
            break;
        }
        outb(0x20, 0x20);
    }

}

