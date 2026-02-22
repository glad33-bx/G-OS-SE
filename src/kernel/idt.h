#ifndef IDT_H
#define IDT_H
#include "io.h"
#include "../lib/gos_types.h"
/*
Intercepter le Clavier (IDT et IRQ)

Pour que le clavier fonctionne, nous devons configurer l'IDT. 
C'est un tableau de pointeurs qui dit au processeur : "Si l'interruption X arrive, exécute cette fonction".
A. L'entrée de l'IDT (Structure)
Chaque entrée fait 8 octets. Voici à quoi elle ressemble :
*/
/* alignement de l'IDT

Le processeur x86 est très pointilleux sur la structure de l'IDT. 
Si elle n'est pas "emballée" (packed), le compilateur ajoute des espaces invisibles et l'adresse devient fausse.
*/

struct idt_entry {
    unsigned short base_low;
    unsigned short sel;        // Segment de code (généralement 0x08)
    unsigned char  always0;
    unsigned char  flags;      // 0x8E pour une interruption 32-bit
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

struct regs {
    uint32_t gs, fs, es, ds;                          // Poussés manuellement
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Poussés par pusha
    uint32_t int_no, err_code;                        // Poussés par le wrapper
    uint32_t eip, cs, eflags, useresp, ss;            // Poussés par le CPU
} __attribute__((packed));

#define PIC1          0x20		/* Adresse du PIC Maître */
#define PIC2          0xA0		/* Adresse du PIC Esclave */
#define PIC1_COMMAND  PIC1
#define PIC1_DATA     (PIC1+1)
#define PIC2_COMMAND  PIC2
#define PIC2_DATA     (PIC2+1)

extern struct idt_entry idt[256];
extern struct idt_ptr idtp;

void init_idt();
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) ;
void PIC_remap() ;
void idt_load();
#endif