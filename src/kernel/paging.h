#ifndef PAGING_H
#define PAGING_H

#include "../lib/gos_types.h"

/* Taille d'une page standard sur x86 */
#define PAGE_SIZE 4096

/* Initialise les structures de données (Directory et Tables) */
void init_paging();

/* Fonctions définies dans paging_asm.asm */
extern void load_page_directory(uint32_t* directory);
extern void enable_paging();

#endif