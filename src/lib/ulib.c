#include "ulib.h"
#include "../kernel/syscall_nums.h" // On inclut les définitions
#include "gos_stdarg.h"


void putc_syscall(char c) {
    asm volatile(
        "int $0x80"
        : 
        : "a"(SYS_PUTC), "b"((int)c) 
        : "memory"
    );
}

void print(char* msg) {
    asm volatile (
        "int $0x80"
        : 
        : "a"(SYS_PRINT), "b"(msg)
        : "memory"
    );
}


void print_color(char* msg, int color) {
    asm volatile(
        "int $0x80"
        : 
        : "a"(SYS_PRINT_COLOR), "b"(msg), "c"(color)
        : "memory"
    );
}

void clear_screen_syscall() {
    asm volatile(
        "int $0x80"
        : 
        : "a"(SYS_CLEAR)
        : "memory"
    );
}

void exit(int code) {
    asm volatile (
        "int $0x80"
        : 
        : "a"(SYS_EXIT), "b"(code)
        : "memory"
    );
}

// --- Fonctions utilitaires internes pour printf ---
static void printf_putd(int n) {
    if (n < 0) {
        putc_syscall('-');
        n = -n;
    }
    if (n / 10) printf_putd(n / 10);
    putc_syscall((n % 10) + '0');
}


static void printf_putx(unsigned int n) {
    char *hex = "0123456789ABCDEF";
    if (n / 16) printf_putx(n / 16);
    putc_syscall(hex[n % 16]);
}

static void printf_puts(char *s) {
    if (!s) s = "(null)";
    while (*s) {
        putc_syscall(*s++);
    }
}

// --- La fonction printf officielle pour tes apps ---
void printf(char *format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 's': printf_puts(va_arg(args, char *)); break;
                case 'd': printf_putd(va_arg(args, int)); break;
                case 'x': 
                    printf_puts("0x");
                    printf_putx(va_arg(args, unsigned int)); 
                    break;
                case 'c': putc_syscall((char)va_arg(args, int)); break;
                case '%': putc_syscall('%'); break;
                default:  putc_syscall(format[i]); break;
            }
        } else {
            putc_syscall(format[i]);
        }
    }
    va_end(args);
}

char getc() {
    int c;
    asm volatile(
        "int $0x80"
        : "=a"(c)        // On dit à GCC que le résultat revient dans EAX
        : "a"(SYS_GETC)  // On met l'ID du syscall dans EAX
        : "memory"
    );
    return (char)c;
}

int sys_read_file(const char* name, void* buffer, unsigned int size) {
    int res;
    asm volatile(
        "int $0x80"
        : "=a"(res) 
        : "a"(SYS_READ_FILE), "b"(name), "c"(buffer), "d"(size) // 10 = SYS_READ_FILE
        : "memory"
    );
    return res;
}

/*int sys_read_file(const char* path, char* buffer, int size) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ_FILE), // EAX
          "b"(path),          // EBX : Adresse du nom
          "c"(buffer),        // ECX : Adresse du buffer <--- VERIFIE ICI
          "d"(size)           // EDX : Taille
    );
    return ret;
}*/

void get_command_line(char* buf) {
    asm volatile("int $0x80" : : "a"(SYS_GET_ARGV), "b"(buf) : "memory");
}


int sys_ls(const char* path, char* buffer) {
    int ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(SYS_LS), "b"(path), "c"(buffer));
    return ret;
}

void sys_set_color(uint8_t fg, uint8_t bg) {
    asm volatile("int $0x80" 
                 : 
                 : "a"(SYS_SET_COLOR), "b"(fg), "c"(bg));
}


