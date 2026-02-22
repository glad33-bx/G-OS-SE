#include "mem.h"

/*
Gestion de la Mémoire : Le Memory Manager (PMM)

Pour l'instant, ton noyau utilise la mémoire "au hasard". 
Un Physical Memory Manager (PMM) permet de savoir quelles zones de la RAM sont libres ou occupées. 
On utilise souvent une Bitmap : un tableau de bits où chaque bit représente une page de 4 Ko de RAM.
*/

// "end" est un symbole magique généré par le linker script (.ld)
// On declare end comme un tableau pour que son nom soit directement l'adresse
extern uint8_t end[];

// On utilise l'ADRESSE du symbole 'end' comme début du tas
//uint32_t placement_address = (uint32_t)&end;
uint32_t placement_address = 0;

// On utilise uintptr_t pour que la taille corresponde TOUJOURS à celle d'un pointeur
static uintptr_t heap_ptr = 0;

void init_mem() {
    // On force la conversion de l'adresse du symbole en entier 32 bits
    placement_address = (uint32_t)end;

    // SECURITE : Si pour une raison X le linker donne une adresse < 1Mo,
    // on force le tas a commencer apres le noyau (0x100000 + 128Ko de marge)
    if (placement_address < 0x100000) {
        placement_address = 0x120000; 
    }
}

uint32_t get_heap_usage() {
    return heap_ptr;
}

void display_mem_info() {
    extern uint32_t placement_address;
    kprintf("\n%vHeap Start :%v 0x%x", VGA_YELLOW, VGA_WHITE, (uint32_t)&end);
    kprintf("\n%vHeap Current:%v 0x%x", VGA_YELLOW, VGA_WHITE, placement_address);
}