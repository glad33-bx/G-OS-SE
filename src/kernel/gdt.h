/*
Dès que tu appelles init_keyboard(), tu actives les interruptions avec sti. 
À cet instant précis :
    Une interruption clavier arrive.
    Le processeur consulte l'IDT.
    L'IDT lui dit : "Saute au code situé dans le segment 0x08".
    Le processeur cherche la définition du segment 0x08 dans la GDT.
    Si tu n'as pas chargé de GDT, le processeur ne trouve rien, panique, et déclenche un Triple Fault (reboot).
*/

#ifndef GDT_H
#define GDT_H

#include "../lib/gos_types.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void init_gdt();

#endif