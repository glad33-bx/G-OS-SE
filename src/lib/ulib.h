#ifndef ULIB_H
#define ULIB_H

#include "gos_types.h"

// Prototypes
void putc_syscall(char c);
void print(char* msg);
void print_color(char* msg, int color);
void sys_set_color(uint8_t fg, uint8_t bg);

void clear_screen_syscall();
void exit(int code);

char getc();

static void printf_putd(int n);
static void printf_putx(unsigned int n);
static void printf_puts(char *s);
void printf(char *format, ...);

void get_command_line(char* buf);

int sys_read_file(const char* name, void* buffer, unsigned int size);
int sys_ls(const char* path, char* buffer);

#endif