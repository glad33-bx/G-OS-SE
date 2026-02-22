#ifndef VGA_H
#define VGA_H

// Répertoire (un tableau de structures) pour les identifier par leur nom
typedef struct {
    const char *name;
    const unsigned char *data;
} font_entry_t;

void load_vga_font();
void vga_set_euro_glyph();
unsigned char utf8_to_cp437(unsigned char c1, unsigned char c2);
void apply_vga_font(const unsigned char *font_data);
void switch_font(char *name);

#endif