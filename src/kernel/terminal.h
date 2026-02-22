#ifndef TERMINAL_H
#define TERMINAL_H

#include "io.h"
#include "../lib/gos_types.h"// pour uint16_t
#include "../lib/gos_stdarg.h" // Indispensable pour les fonctions à arguments variables

#define VGA_WIDTH 80                // X
#define VGA_HEIGHT 25               // Y
#define VIDEO_MEM (char*)0xB8000    // la mémoire vidéo VGA, qui commence à l'adresse physique 0xB8000

// REMPLACE tes variables actuelles par ceci :
extern uint8_t cursor_x;
extern uint8_t cursor_y;
extern uint8_t terminal_color;

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,  // <--- Celle qui te manquait !
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

void scroll();
void clear_screen() ;
void set_color(uint8_t fg, uint8_t bg);

void puts(char *str);
void putc(char c);
void putc_color(char c, uint8_t color);
void putx(unsigned int n) ;
void putd(int n);
void puthex(unsigned char n);
void put_char_at(char c, int x, int y);

void kprintf(char *format, ...);
void kprintf_color(char* d, uint8_t color);

void readline(char* buffer, int max);

void init_terminal();
void terminal_backspace();

void move_cursor_back();
void move_cursor_forward();
//void update_cursor();
void enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void update_hardware_cursor() ;

#endif