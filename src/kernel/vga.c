#include "../lib/config.h"
#include "io.h"
#include "../lib/gos_types.h"
#include "vga.h"
#include "../fnt/lat2_terminus16.h"
#include "../fnt/unifont.h"
#include "../fnt/semi_coder.h"
#include "../fnt/uni.h"
#include "../fnt/uni1_vga16.h"
#include "../fnt/uni2_fixed16.h"
#include "string.h"

// matrice de bits pour dessiner un Euro
unsigned char euro_glyph[16] = {
    0x00, 0x00, 0x1E, 0x33, 0x30, 0x7E, 0x30, 0x7E, 
    0x30, 0x30, 0x33, 0x1E, 0x00, 0x00, 0x00, 0x00
};

// Une police simple (
extern unsigned char gilles_os_font[4096]; 

// On déclare les polices externes (assure-toi que les noms correspondent à tes .h)
extern unsigned char lat2_terminus16[];
extern unsigned char unifont_data[];
extern unsigned char vga16_data[];
extern unsigned char uni2_fixed16[];
extern unsigned char semi_coder[];
extern unsigned char uni[];

font_entry_t font_table[] = {
    {"terminus", lat2_terminus16},
    {"unifont",  unifont_data},
    {"vga16",    uni1_vga16_data},
    {"fixed",    uni2_fixed16},
    {"semi",    semi_coder},
    {"uni",    uni},
    {0, 0} // Fin de tableau
};

/*void load_vga_font() {
    uint8_t *vram = (uint8_t *)0xA0000;

    asm volatile("cli"); // Sécurité maximale pendant le switch de planes

    // 1. Préparer le contrôleur pour l'accès au Plan 2 (Font Plane)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); // Reset synchrone
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); // Écriture Plan 2 uniquement
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x07); // Accès séquentiel
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); // Fin reset

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02);   // Lecture Plan 2
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00);   // Write mode 0
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00);   // VRAM à A0000h

    // 2. Copier proprement : 1 caractère tous les 32 octets
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 16; j++) {
            vram[i * 32 + j] = gilles_os_font[i * 16 + j];
        }
        // On remplit les 16 octets restants du slot par du vide
        for (int j = 16; j < 32; j++) {
            vram[i * 32 + j] = 0;
        }
    }

    // 3. Restaurer le mode texte (B8000h et Plans 0&1)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01);
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); 
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x03); 
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03);

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);   
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10);   
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E);   

    asm volatile("sti");
}*/

/*void load_vga_font() {
    // 1. Préparer le contrôleur pour l'accès au Plan 2
    outb(0x3C4, 0x01); // Reset synchrone
    outb(0x3C5, 0x01);

    outb(0x3C4, 0x02); // Mask Map Register
    outb(0x3C5, 0x04); // On n'écrit que dans le Plan 2

    outb(0x3C4, 0x04); // Sequential Mode
    outb(0x3C5, 0x07); // Accès séquentiel

    outb(0x3CE, 0x05); // Graphics Mode Register
    outb(0x3CF, 0x00); // Mode normal

    outb(0x3CE, 0x06); // Miscellaneous Register
    outb(0x3CF, 0x00); // Map à 0xA0000 (64k)

    // 2. Copier les données de la police vers la mémoire vidéo
    // L'adresse 0xA0000 est l'endroit où le VGA attend les glyphes
    unsigned char* vga_mem = (unsigned char*)0xA0000;
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 16; j++) {
            vga_mem[i* 32 + j] = gilles_os_font[i *16 + j];
        }
    }

    // 3. Rétablir les registres pour le mode texte normal
    outb(0x3C4, 0x02);
    outb(0x3C5, 0x03); // Écriture sur Plans 0 et 1 (Texte et Attributs)

    outb(0x3C4, 0x04);
    outb(0x3C5, 0x03); // Mode "Odd/Even" pour le texte

    outb(0x3CE, 0x04); // Read Plane Select
    outb(0x3CF, 0x00);

    outb(0x3CE, 0x05);
    outb(0x3CF, 0x10); // Rétablir le mode "Odd/Even"

    outb(0x3CE, 0x06);
    outb(0x3CF, 0x0E); // Mapper la mémoire texte à 0xB8000
}*/

/*
void load_vga_font() {
    uint8_t *vram = (uint8_t *)0xA0000;

    // 1. Déconnecter l'affichage pour éviter les parasites visuels
    asm volatile("cli");

    // Séquence de registres pour accéder au Plan 2 (Font Plane)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); // Reset synchrone
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); // Écriture Plan 2
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x07); // Accès séquentiel
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); // Fin Reset

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02);   // Lecture Plan 2
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00);   // Write mode 0
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00);   // VRAM à A0000h

    // 2. Copie avec le saut de 32 octets
    // gilles_os_font contient 256 caractères de 16 octets = 4096 octets
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 16; j++) {
            // On place le glyphe (16 octets) au début du slot de 32 octets
            vram[i * 32 + j] = gilles_os_font[i * 16 + j];
        }
        // Les 16 octets restants du slot de 32 sont mis à 0
        for (int j = 16; j < 32; j++) {
            vram[i * 32 + j] = 0;
        }
    }

    // 3. Restaurer le mode texte standard (Plan 0 & 1 à B8000h)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01);
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); 
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x03); 
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03);

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);   
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10);   
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E);   

    asm volatile("sti");
}
*/

void load_vga_font() {
    // Par défaut au démarrage, on charge la première de la liste
    apply_vga_font(font_table[1].data);
}

/*void vga_set_euro_glyph() {
    unsigned char *vram = (unsigned char *)0xA0000; // Adresse de la font en Plane 2
    unsigned int char_index = 0xEE; // On remplace l'epsilon grec

    // 1. Débloquer l'accès à la mémoire de la police
    outb(VGA_SEQ_INDEX, 0x01); outb(VGA_SEQ_DATA, 0x01); // Reset asynchrone
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); // Write to plane 2
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x07); // Sequential access
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00); // Write mode 0
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00); // VRAM at A0000h

    // 2. Copier les 16 octets du glyphe
    // Chaque caractère occupe 32 octets en mémoire (on n'en utilise que 16)
    for (int i = 0; i < 16; i++) {
        vram[char_index * 32 + i] = euro_glyph[i];
    }

    // 3. Restaurer le mode texte standard
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); // Write to plane 0 & 1
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x03); // Enable planes
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02); // Read plane 2
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10); // Odd/Even mode
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E); // VRAM at B8000h
}*/
void vga_set_euro_glyph() {
    // ÉTAPE CRITIQUE : Désactiver les interruptions.
    // Si le timer ou le clavier essaie d'écrire à l'écran pendant qu'on est en Plane 2, c'est le crash.
    asm volatile("cli");

    unsigned char *vram = (unsigned char *)VIDEO_FONT_ADDR; // 0xA0000
    //unsigned int char_index = 0xEE; 

    // 1. Débloquer l'accès à la mémoire de la police (Plane 2)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); // Reset Synchrone
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); // Write to plane 2
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x07); // Sequential access
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); // Fin Reset

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02);   // Read plane 2
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00);   // Write mode 0
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00);   // VRAM at A0000h (64k)

    // 2. Copier les 16 octets du glyphe
    for (int i = 0; i < 16; i++) {
//        vram[CHAR_EURO * 32 + i] = euro_glyph[i];
        if (i < 16)
                vram[CHAR_EURO * 32 + i] = euro_glyph[i];
            else
                vram[CHAR_EURO * 32 + i] = 0;
    }

    // 3. RESTAURATION COMPLÈTE du mode texte (C'est ici que ça jouait)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); // Reset
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); // Write to plane 0 & 1
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x03); // Enable Odd/Even
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); // Fin Reset

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);   // RESTORE: Read plane 0
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10);   // RESTORE: Odd/Even mode
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E);   // RESTORE: VRAM at B8000h

    // Réactiver les interruptions
    asm volatile("sti");
}

// Modification de load_vga_font pour accepter une police spécifique
void apply_vga_font(const unsigned char *font_data) {
    if (!font_data) return;
    
    uint8_t *vram = (uint8_t *)0xA0000;
    asm volatile("cli");

    // --- Séquence VGA identique à celle qu'on a corrigée ---
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    outb(0x3C4, 0x04); outb(0x3C5, 0x07);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);
    outb(0x3CE, 0x06); outb(0x3CF, 0x00);

    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 16; j++) {
            vram[i * 32 + j] = font_data[i * 16 + j];
        }
    }

    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x04); outb(0x3C5, 0x03);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
    asm volatile("sti");
}

void switch_font(char *name) {
    for (int i = 0; font_table[i].name != 0; i++) {
        if (strcmp(name, font_table[i].name) == 0) {
            apply_vga_font(font_table[i].data);
            return;
        }
    }
    kprintf("Police '%s' non trouvee.\n", name);
}