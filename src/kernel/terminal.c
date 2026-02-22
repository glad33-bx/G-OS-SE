#include "terminal.h"
#include "keyboard.h"
#include "../lib/config.h"
#include "vga.h"

/*
En mode texte VGA, chaque caractère occupe 2 octets : 
le premier pour le code ASCII, 
le second pour l'attribut (couleur de fond et de texte).
*/

static uint16_t* video_memory = (uint16_t*)VIDEO_ADDR; // Pointeur vers la mémoire vidéo VGA (80 colonnes x 25 lignes)
uint8_t terminal_color = 0x0F; // Blanc brillant sur Noir

// Variables pour suivre la position du curseur
uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

// Variable globale pour la couleur actuelle (Défaut : Gris clair sur noir 0x07)
unsigned char current_color = 0x07;

static int utf8_state = 0;

// Petite fonction helper nécessaire pour init_terminal
void set_color(uint8_t fg, uint8_t bg) {
    terminal_color = fg | (bg << 4);
}

void put_char_at(char c, int x, int y) {
    // Calcul de l'offset dans la mémoire VGA (80 colonnes * 2 octets par caractère)
    const int index = (y * 80 + x);
    uint16_t *terminal_buffer = (uint16_t *)VIDEO_ADDR;
    
    // On garde la couleur actuelle (on récupère l'octet de poids fort)
    uint16_t current_val = terminal_buffer[index];
    uint8_t color = (current_val >> 8);
    
    terminal_buffer[index] = (uint16_t)c | (uint16_t)color << 8;
}

void puthex(unsigned char n) {
    char* hex = "0123456789ABCDEF";
    putc(hex[(n >> 4) & 0xF]);
    putc(hex[n & 0xF]);
    putc(' ');
}

// Affiche un nombre entier
void putd(int n) {
    if (n == 0) {
        putc('0');
        return;
    }
    if (n < 0) {
        putc('-');
        n = -n;
    }
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    while (n > 0) {
        buf[--i] = (n % 10) + '0';
        n /= 10;
    }
    puts(&buf[i]);
}

/*void putc(char c) {
    uint16_t *terminal_buffer = (uint16_t *)VIDEO_ADDR;
    
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        int index = cursor_y * SCREEN_WIDTH + cursor_x;
        // ON UTILISE terminal_color ICI, car set_color modifie terminal_color
        terminal_buffer[index] = (uint16_t)c | (uint16_t)terminal_color << 8;
        cursor_x++;
    }

    if (cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= SCREEN_HEIGHT) {
        scroll();
        cursor_y = SCREEN_HEIGHT-1;
    }
}*/

void putc(char c) {
    uint8_t uc = (uint8_t)c;
    uint16_t *terminal_buffer = (uint16_t *)VIDEO_ADDR;

    // --- LOGIQUE DE DÉCODAGE UTF-8 ---
    if (uc == 0xC3) {
        utf8_state = 1; // On a détecté un début d'accent, on ignore cet octet
        return;
    }

    if (utf8_state == 1) {
        utf8_state = 0;
        // On traduit l'octet de suite UTF-8 en index de ta police
        // Ces codes correspondent à la table CP437/Latin-2 classique
        switch(uc) {
            case 0xA9: uc = 0x82; break; // é
            case 0xA8: uc = 0x8A; break; // è
            case 0xA0: uc = 0x85; break; // à
            case 0xA2: uc = 0x83; break; // â
            case 0xA7: uc = 0x87; break; // ç
            case 0xAA: uc = 0x88; break; // ê
            case 0xAB: uc = 0x89; break; // ë
            case 0xAF: uc = 0x8F; break; // Å (souvent utilisé pour tests)
            default:   uc = '?';  break; // Caractère inconnu
        }
    }
    // ---------------------------------

    if (uc == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        int index = cursor_y * SCREEN_WIDTH + cursor_x;
        // On utilise 'uc' (le caractère potentiellement traduit)
        terminal_buffer[index] = (uint16_t)uc | (uint16_t)terminal_color << 8;
        cursor_x++;
    }

    // Gestion du débordement horizontal
    if (cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    // Gestion du scroll vertical
    if (cursor_y >= SCREEN_HEIGHT) {
        scroll();
        cursor_y = SCREEN_HEIGHT - 1;
    }
    
    update_hardware_cursor(); // N'oublie pas de mettre à jour le curseur clignotant
}

void puts(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putc(str[i]);
    }
}

// Affiche un nombre en hexadécimal (utile pour la mémoire et les scancodes)
void putx(unsigned int n) {
    char *hex = "0123456789ABCDEF";
    char buf[9];
    int i = 8;
    buf[i] = '\0';
    if (n == 0) {
        puts("0");
        return;
    }
    while (n > 0) {
        buf[--i] = hex[n % 16];
        n /= 16;
    }
    puts(&buf[i]);
}

void kprintf(char *format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 's': { // CHAÎNE
                    char *s = va_arg(args, char *);
                    if (s == 0) s = "(null)"; // Sécurité pour les pointeurs nuls
                    puts(s);
                    break;
                }
                case 'd': { // NOMBRE ENTIER
                    int d = va_arg(args, int);
                    putd(d);
                    break;
                }
                case 'x': { // HEXADÉCIMAL (pratique pour le debug)
                    unsigned int x = va_arg(args, unsigned int);
                    putx(x); // Utilise la fonction putx qu'on a vu avant
                    break;
                }
                case 'v': { // TA COULEUR PERSONNALISÉE
                    uint8_t color = (uint8_t)va_arg(args, int);
                    set_color(color, VGA_BLACK);
                    break;
                }
                case 'c': { // UN SEUL CARACTÈRE
                    char c = (char)va_arg(args, int);
                    putc(c);
                    break;
                }
                default:
                    putc(format[i]);
                    break;
            }
        } else {
            putc(format[i]);
        }


    }
    va_end(args);
    update_hardware_cursor();
}

void kprintf_color(char* d, uint8_t color) {
    for (int i = 0; d[i] != '\0'; i++) {
        putc_color(d[i], color);
    }
}

// Une version de putc qui accepte la couleur
void putc_color(char c, uint8_t color) {
    extern uint16_t* video_memory; // Généralement (uint16_t*)VIDEO_ADDR
    extern uint32_t cursor_pos;

    if (c == '\n') {
        // Logique pour le retour à la ligne (dépend de ton implémentation)
        cursor_pos += 80 - (cursor_pos % SCREEN_HEIGHT);
    } else {
        // On combine le caractère et la couleur
        // Format : [ Background (4b) | Foreground (4b) | ASCII (8b) ]
        video_memory[cursor_pos] = (uint16_t)c | (uint16_t)color << 8;
        cursor_pos++;
    }
    // N'oublie pas de gérer le défilement (scrolling) si cursor_pos > 2000
}

void readline(char* buffer, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = keyboard_getc(); // getc(); 

        if (c == 0) continue; 

        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            kprintf("\n");
            update_hardware_cursor(); // On synchronise après le saut de ligne
            return;
        }

        if (c == '\b') { // Touche Retour arrière
            if (i > 0) {
                i--;
                // 1. Reculer le curseur logique
                cursor_x--;
                // 2. Effacer visuellement (afficher un espace là où était le curseur)
                putc(' '); 
                // 3. Reculer à nouveau car putc(' ') a avancé le curseur d'une case
                cursor_x--;
                // 4. Mettre à jour le curseur clignotant matériel
                update_hardware_cursor();
            }
            continue;
        }

        if (i < max - 1) {
            buffer[i++] = c;
            putc(c); 
            update_hardware_cursor(); // On suit le caractère tapé
        }
    }
    buffer[i] = '\0';
}

// Dans terminal.c ou video.c
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    // On force le cast en uint8_t pour éviter toute extension de signe
    uint16_t data = (uint16_t) (uint8_t)c | (uint16_t) color << 8;
    video_memory[index] = data;
}

void scroll() {
    uint16_t* video_memory = (uint16_t*)VIDEO_ADDR;

    // On commence à la ligne 1 pour laisser l'horloge intacte en ligne 0
    // On décale tout d'une ligne vers le haut
    for (int y = 0; y < (SCREEN_HEIGHT - 1); y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            video_memory[y * SCREEN_WIDTH + x] = video_memory[(y + 1) * 80 + x];
        }
    }

    // On efface la toute dernière ligne (ligne 24)
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        video_memory[(SCREEN_HEIGHT - 1) * 80 + x] = (uint16_t)' ' | (uint16_t)(terminal_color << 8);
    }
}

void clear_screen() {
    char *video_memory = VIDEO_MEM;
    
    // 80 colonnes * 25 lignes * 2 octets par caractère
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        video_memory[i] = ' ';      // Caractère espace
        video_memory[i+1] = 0x07;   // Attribut : Gris clair sur noir
    }
    
    // On n'oublie pas de remettre les coordonnées du curseur à zéro
    cursor_x = 0;
    cursor_y = 0;
}

/*
Le curseur matériel (le petit trait qui clignote) est géré par le contrôleur VGA. 
Si tu veux vraiment un écran "propre", tu peux ajouter cette petite fonction dans terminal.c 
(en utilisant tes fonctions outb) pour le déplacer ou le masquer.
*/
/*
Pourquoi le curseur est-il si spécial ?

Le curseur que tu vois clignoter n'est pas un simple caractère écrit en mémoire. 
C'est un composant matériel du contrôleur VGA. Pour le déplacer, on ne peut pas simplement écrire à l'adresse VIDEO_ADDR.

On doit envoyer des commandes aux ports E/S (I/O Ports) :
    Le port 0x3D4 sert d'index (on dit au matériel quel registre on veut modifier).
    Le port 0x3D5 reçoit la valeur.
*/
/*
Le curseur est géré par le contrôleur VGA via les ports 0x3D4 et 0x3D5. 
Tu dois mettre à jour sa position à chaque fois que tu écris un caractère.
*/
/*
certains BIOS ou émulateurs désactivent le curseur par défaut.
Cette fonction configure les registres du contrôleur CRT (0x3D4) pour définir la forme du curseur (ligne de scan 14 à 15).
*/
void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
 
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

// voici le code standard pour envoyer la position du curseur aux ports I/O de la carte VGA (Ports 0x3D4 et 0x3D5)
void update_hardware_cursor() {
    // On utilise tes variables globales cursor_y et cursor_x
    uint16_t pos = (cursor_y * SCREEN_WIDTH) + cursor_x; 

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/*
Son rôle est de préparer la mémoire vidéo VGA (qui commence à l'adresse VIDEO_ADDR) pour afficher du texte.

Dans un noyau, cette fonction doit faire deux choses :

    Initialiser les variables de suivi (position du curseur x et y, et couleur actuelle).

    Effacer l'écran pour qu'il soit totalement noir au démarrage.
Comment ça fonctionne ?

    L'adresse VIDEO_ADDR : C'est une zone "magique" de la RAM. Tout ce que tu écris ici est directement envoyé à l'écran par la carte graphique.

    Le format uint16_t : Chaque case de l'écran prend 2 octets (16 bits) :

        L'octet de poids faible (8 bits) est le caractère ASCII (ex: 'A').

        L'octet de poids fort (8 bits) contient les couleurs (4 bits pour le texte, 4 bits pour le fond).

    L'effacement : On parcourt toute la grille (2000 cellules) pour y mettre un espace vide avec notre couleur de fond préférée.
*/

void init_terminal() {
    cursor_x = 0;
    cursor_y = 0;
    
    // On définit la couleur par défaut (Texte Blanc sur fond Noir)
    set_color(VGA_WHITE, VGA_BLACK);

    // On efface l'écran : 80 colonnes * 25 lignes = 2000 caractères
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        // On remplit avec un espace ' ' et la couleur actuelle
        video_memory[i] = (uint16_t)' ' | (uint16_t)terminal_color << 8;
    }
}

void terminal_backspace() {
    if (cursor_x > 2) { // Empêche d'effacer le prompt "> "
        cursor_x--;
        uint16_t* video_mem = (uint16_t*)VIDEO_ADDR;
        video_mem[cursor_y * SCREEN_WIDTH + cursor_x] = (uint16_t)' ' | (uint16_t)(VGA_WHITE << 8);
        update_hardware_cursor();
    }
}

// Ton curseur matériel doit bouger sans effacer de texte pour les flèches Gauche/Droite.
void move_cursor_back() {
    if (cursor_x > 2) { // Ne pas dépasser le "> "
        cursor_x--;
        update_hardware_cursor();
    }
}

void move_cursor_forward() {
    if (cursor_x < (SCREEN_HEIGHT - 1)) {
        cursor_x++;
        update_hardware_cursor();
    }
}


