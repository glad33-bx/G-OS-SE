#ifndef SYSCALL_NUMS_H
#define SYSCALL_NUMS_H

/*#define SYS_PRINT       1
#define SYS_CLEAR       2
#define SYS_PRINT_COLOR 3
#define SYS_PUTC        4
#define SYS_EXIT        5
#define SYS_GETC        6
#define SYS_GOTOXY      7
#define SYS_READ_FILE   8
#define SYS_GET_ARGV    11*/

typedef enum {
    SYS_NONE = 0,      // Souvent 0 n'est pas utilisé
    SYS_PRINT,
    SYS_CLEAR,
    SYS_PRINT_COLOR,
    SYS_PUTC,
    SYS_EXIT,
    SYS_GETC,
    SYS_GOTOXY,
    SYS_READ_FILE,
    SYS_EXEC,
    SYS_LS,
    SYS_GET_ARGV,
    SYS_SET_COLOR
} syscall_t;

#endif