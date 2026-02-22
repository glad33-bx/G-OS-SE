#ifndef ULIB_H
#define ULIB_H

// Définition des types standard pour l'userland
typedef unsigned int   uint32_t;
typedef int            int32_t;
typedef unsigned short uint16_t;
typedef short          int16_t;
typedef unsigned char  uint8_t;
typedef char           int8_t;

// Tes prototypes
void printf(char *format, ...);
void print(char* msg);
void print_color(char* msg, int color);
void putc_syscall(char c);
void exit(int code);
char getc();

#endif