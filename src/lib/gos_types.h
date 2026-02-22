#ifndef TYPES_H
#define TYPES_H

/* Définitions des types standards pour un noyau x86 32 bits */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* Types pour les tailles et pointeurs */
typedef uint32_t           size_t;
typedef uint32_t           uintptr_t;

// Ajoute ceci pour remplacer <stdbool.h>
typedef _Bool bool;
#define true 1
#define false 0

// Ajoute ceci pour remplacer <stddef.h> (NULL)
#define NULL ((void*)0)

#endif