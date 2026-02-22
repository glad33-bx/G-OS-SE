#ifndef GOS_MEMORY_H
#define GOS_MEMORY_H

#include "../lib/gos_types.h"
#include "terminal.h"
#include "../lib/config.h"

typedef struct gos_mem_header {
    unsigned int size;
    int is_free;
    struct gos_mem_header *next;
} __attribute__((packed, aligned(4))) gos_mem_header_t; //gos_mem_header_t;

void init_memory();
void *kmalloc(unsigned int size);
void kfree(void *ptr);

#endif