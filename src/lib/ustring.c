#include "ustring.h"
#include "gos_types.h"

static char* next_token = 0; // strtok

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
    while (s[len]) 
        len++;
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


char* strrchr(const char* s, int c) {
    char* last = NULL;
    char target = (char)c;

    // On parcourt toute la chaîne jusqu'au caractère nul '\0'
    do {
        if (*s == target) {
            last = (char*)s; // On mémorise la position actuelle
        }
    } while (*s++);

    return last;
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest;

    // 1. On déplace le pointeur jusqu'à la fin de la chaîne de destination
    while (*ptr != '\0') {
        ptr++;
    }

    // 2. On copie la source à partir de cet emplacement
    while (*src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
    }

    // 3. On ajoute le caractère nul final pour terminer la nouvelle chaîne
    *ptr = '\0';

    return dest;
}

// fonction pour connaître la taille d'un nombre
int get_int_len(int n) {
    if (n == 0) return 1;
    int len = 0;
    if (n < 0) { len++; n = -n; }
    while (n > 0) { 
        n /= 10; len++; 
    }
    return len;
}

char* strtok(char* str, const char* delim) {
    // Si str est fourni, on commence une nouvelle chaîne
    // Sinon, on continue là où on s'était arrêté (next_token)
    if (str) {
        next_token = str;
    }

    // Si on n'a plus rien à lire, on s'arrête
    if (!next_token || *next_token == '\0') {
        return 0;
    }

    // 1. Sauter les délimiteurs au début (ex: s'il y a plusieurs //)
    char* start = next_token;
    while (*start != '\0') {
        int is_delim = 0;
        for (int i = 0; delim[i] != '\0'; i++) {
            if (*start == delim[i]) {
                is_delim = 1;
                break;
            }
        }
        if (!is_delim) break;
        start++;
    }

    // Si on a atteint la fin de la chaîne en ne trouvant que des délimiteurs
    if (*start == '\0') {
        next_token = 0;
        return 0;
    }

    // 2. Trouver la fin du token actuel
    char* end = start;
    while (*end != '\0') {
        int is_delim = 0;
        for (int i = 0; delim[i] != '\0'; i++) {
            if (*end == delim[i]) {
                is_delim = 1;
                break;
            }
        }
        if (is_delim) break;
        end++;
    }

    // 3. Préparer le prochain appel
    if (*end == '\0') {
        next_token = 0; // C'était le dernier morceau
    } else {
        *end = '\0';    // On "coupe" la chaîne ici en insérant un terminateur
        next_token = end + 1;
    }

    return start;
}

int hexval(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

long atol(const char *num) {
    long value = 0;
    int neg = 0;
    if (num[0] == '0' && num[1] == 'x') {
        // hex
        num += 2;
        while (*num && isxdigit(*num))
            value = value * 16 + hexval(*num++);
    } else {
        // decimal
        if (num[0] == '-') {
            neg = 1;
            num++;
        }
        while (*num && isdigit(*num))
            value = value * 10 + *num++  - '0';
    }
    if (neg)
        value = -value;
    return value;
}

int atoi(const char* s) {
    int res = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}


int isdigit(int c){
	return (unsigned)c - '0' < 10;
}

int isxdigit(int c){
	return isdigit(c) || ((unsigned)c | 32) - 'a' < 6;
}



