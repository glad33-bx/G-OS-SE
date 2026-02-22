#ifndef CONFIG_H
#define CONFIG_H

// --- Informations Système ---
#define OS_NAME "G-OS-SE"
#define OS_VERSION "0.01"
#define DEBUG 0

// --- Adresses Matérielles Vidéo (VGA) ---
#define VIDEO_ADDR         0xB8000  // Adresse de base du mode texte
#define VIDEO_FONT_ADDR    0xA0000  // Adresse pour charger les polices (Plane 2)
#define SCREEN_WIDTH       80
#define SCREEN_HEIGHT      25
#define SCREEN_SIZE        (SCREEN_WIDTH * SCREEN_HEIGHT)

// --- Ports I/O VGA (Registres) ---
#define VGA_CTRL_REGISTER  0x3D4    // Registre d'index
#define VGA_DATA_REGISTER  0x3D5    // Registre de données
#define VGA_MISC_OUTPUT    0x3C2
#define VGA_SEQ_INDEX      0x3C4
#define VGA_SEQ_DATA       0x3C5
#define VGA_GC_INDEX       0x3CE
#define VGA_GC_DATA        0x3CF

// --- Gestion Mémoire ---
#define PAGE_SIZE          4096     // 4 Ko
#define KERNEL_STACK_SIZE  0x4000   // 16 Ko pour la pile noyau
#define APP_LOAD_ADDR      0x500000 // 5 Mo (où tu charges tes apps fat_read)

// --- Paramètres du Shell et Clavier ---
#define BUFFER_SIZE        256      // Taille max d'une ligne de commande
#define HISTORY_SIZE       10       // Nombre de commandes mémorisées
#define TAB_SIZE           4

// --- Couleurs par défaut ---
#define DEFAULT_COLOR_FG   0x0F     // Blanc brillant
#define DEFAULT_COLOR_BG   0x00     // Noir

#define CHAR_EURO 0xEE  // On remplace l'epsilon (238) par l'Euro

#define MEM_START 0x400000
#define MEM_TOTAL_SIZE 0x1000000 


#endif
