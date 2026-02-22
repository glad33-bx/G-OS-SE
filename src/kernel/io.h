/* h/io.h */
#ifndef IO_H
#define IO_H

#include "../lib/gos_types.h"
#include "terminal.h"

// Déclarations des fonctions définies dans io.asm
/*unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char data);

static inline void outw(uint16_t port, uint16_t data);
static inline uint16_t inw(uint16_t port) ;
*/

void reboot(void);

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outw(uint16_t port, uint16_t data) {
    __asm__ __volatile__("outw %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ __volatile__("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void io_wait(void) {
    // Port 0x80 est souvent utilisé pour un délai d'environ 1 à 4 microsecondes
    // car l'écriture sur ce port prend un cycle de bus complet sans rien affecter.
    asm volatile ( "outb %%al, $0x80" : : "a"(0) );
}

#endif