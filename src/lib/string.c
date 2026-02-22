#include "string.h"
#include "gos_types.h"

// Vérifie si deux chaînes sont parfaitement identiques
int str_equal(char *s1, char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

// Vérifie si s1 commence par s2 (utile pour "touch fichier.txt")
int str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    size_t len_pre = strlen(prefix);
    size_t len_str = strlen(str);
    return len_str < len_pre ? 0 : memcmp(prefix, str, len_pre) == 0;
}

/**
 * Remplit les n premiers octets de la zone mémoire s avec l'octet c.
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    
    while (n--) {
        *p++ = (unsigned char)c;
    }
    
    return s;
}

// pour copier des noms de fichiers ou des buffers
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) {
            return 0;
        }
    }
    return (char *)s;
}

int memcmp(const void *s1, const void *s2, uint32_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}


// Version standard de comparaison
int str_compare(char *s1, char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

uint32_t strlen(const char *s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

void trim(char* str) {
    if (str == NULL) return;

    // 1. Enlever les espaces au début
    char* start = str;
    while (*start == ' ') start++;

    if (start != str) {
        char* dest = str;
        while (*start) {
            *dest++ = *start++;
        }
        *dest = '\0';
    }

    // 2. Enlever les espaces à la fin
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

char* strncpy(char* dest, const char* src, uint32_t n) {
    char* d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

/**
 * Convertit un entier en chaîne hexadécimale
 * @param dest: Le buffer de destination
 * @param num: Le nombre à convertir
 */
void itoa_hex(char* dest, uint32_t num) {
    char* digits = "0123456789ABCDEF";
    int i = 0;

    // Si le nombre est 0, on le gère directement
    if (num == 0) {
        dest[i++] = '0';
        dest[i] = '\0';
        return;
    }

    // On extrait les chiffres de droite à gauche
    while (num > 0) {
        dest[i++] = digits[num % 16];
        num = num / 16;
    }
    dest[i] = '\0';

    // La chaîne est à l'envers, on doit l'inverser
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char tmp = dest[start];
        dest[start] = dest[end];
        dest[end] = tmp;
        start++;
        end--;
    }
}

void kprint_hex(uint32_t num) {
    char* digits = "0123456789ABCDEF";
    char hex_str[11]; // "0x" + 8 caractères + '\0'
    
    hex_str[0] = '0';
    hex_str[1] = 'x';
    hex_str[10] = '\0';

    // On remplit de droite à gauche pour garder les zéros non significatifs
    for (int i = 9; i >= 2; i--) {
        hex_str[i] = digits[num % 16];
        num /= 16;
    }

    kprintf(hex_str);
}

// fonction qui convertit en base 10
void itoa(char* dest, uint32_t n) {
    int i = 0;
    if (n == 0) {
        dest[i++] = '0';
        dest[i] = '\0';
        return;
    }

    // Extraire les chiffres
    while (n > 0) {
        dest[i++] = (n % 10) + '0';
        n /= 10;
    }
    dest[i] = '\0';

    // Inverser la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = dest[j];
        dest[j] = dest[i - j - 1];
        dest[i - j - 1] = tmp;
    }
}

char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}


