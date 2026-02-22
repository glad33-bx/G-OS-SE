#ifndef MEM_H
#define MEM_H

#include "../lib/gos_types.h"
#include "gos_memory.h"
#include "terminal.h"

/**
 * kmalloc - Alloue une zone de mémoire sur le tas (heap).
 * @size: Nombre d'octets à allouer.
 * Retourne un pointeur vers la zone allouée.
 */
//void* kmalloc(uint32_t size);

/**
 * get_heap_usage - Optionnel : retourne l'adresse actuelle du tas
 * Utile pour débugger l'occupation mémoire.
 */
uint32_t get_heap_usage();


void init_mem();
void display_mem_info();

#endif