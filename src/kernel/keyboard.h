#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../lib/gos_types.h"
#include "terminal.h"
#include "shell.h"
#include "../lib/string.h"
#include "io.h"
#include "idt.h"

#define BUFFER_SIZE 256

// On dit au compilateur : "Ces variables existent quelque part ailleurs"
extern unsigned char kbd_map[128];
extern unsigned char kbd_map_shift[128];
extern int shift_pressed;

char keyboard_getc();

void reset_buffer();
void execute_command();

void init_keyboard();

void load_history(int direction);
void refresh_line();

void keyboard_handler();
void handle_extended_keys(uint8_t scancode);
void keyboard_set_layout(char* lang);

void move_cursor_left();
void sync_cursor_visual();

void handle_backspace();
void insert_char(char c);
void handle_delete() ;

#endif