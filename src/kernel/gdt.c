#include "gdt.h"

struct gdt_entry gdt[3];
struct gdt_ptr gp;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void init_gdt() {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Segment Nul
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Segment Code (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Segment Data (0x10)

    // Chargement de la GDT en assembleur inline
    __asm__ __volatile__("lgdt (%0)" : : "r" (&gp));
    
    // Rechargement des registres de segments
    __asm__ __volatile__(
        "movw $0x10, %ax\n"
        "movw %ax, %ds\n"
        "movw %ax, %es\n"
        "movw %ax, %fs\n"
        "movw %ax, %gs\n"
        "movw %ax, %ss\n"
        "ljmp $0x08, $.1\n"
        ".1:\n"
    );
}