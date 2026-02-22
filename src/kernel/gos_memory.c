#include "../lib/config.h"
#include "gos_memory.h"

/* On utilise ici gos_mem_header_t qui DOIT être défini 
   complètement dans gos_memory.h 
*/
static gos_mem_header_t *first_block = (gos_mem_header_t *)MEM_START;
static int memory_is_ready = 0; // Notre flag de sécurité

/*Le piège des adresses (MEM_START)

Si l'initialisation échoue toujours (plantage immédiat), vérifie la valeur de MEM_START.
Si ton noyau est chargé à 0x100000 (1 Mo), assure-toi que MEM_START est bien après la fin de ton noyau.
Si MEM_START pointe sur une zone déjà utilisée par le code du noyau, 
init_memory va écraser ton propre code et provoquer un plantage total.
*/

void init_memory() {
    // ON NE REDÉCLARE PAS first_block ici, on utilise la globale
    first_block = (gos_mem_header_t *)MEM_START;

    first_block->size = MEM_TOTAL_SIZE - sizeof(gos_mem_header_t);
    first_block->is_free = 1; 
    first_block->next = NULL;
    
    memory_is_ready = 1;
    kprintf("[MEM] Tas initialise a 0x%x (%d octets)\n", MEM_START, first_block->size);
}

void *kmalloc(unsigned int size) {
    if (!memory_is_ready) return NULL;

    // Aligne la taille sur 4 octets
    size = (size + 3) & ~3; 

    gos_mem_header_t* curr = first_block;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = 0;
            // On renvoie l'adresse alignée juste après le header
            return (void*)(curr + 1); 
        }
        curr = curr->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    /* On recule la position du pointeur pour retrouver le header */
    gos_mem_header_t *header = (gos_mem_header_t *)ptr - 1;
    header->is_free = 1;

    if (header->next && header->next->is_free) {
        header->size += sizeof(gos_mem_header_t) + header->next->size;
        header->next = header->next->next;
    }
}