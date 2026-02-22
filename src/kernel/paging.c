/*
Activer la pagination, c'est comme passer d'une carte papier (mémoire physique) à un système de coordonnées GPS (mémoire virtuelle). 
Le processeur ne parlera plus directement à la RAM, mais passera par une table de traduction.

Pour cela, nous avons besoin de deux structures :

    Page Directory : Le catalogue principal (1024 entrées).

    Page Table : Le détail pour chaque section (1024 entrées par table).
    On va créer un annuaire qui pointe vers lui-même (Identity Mapping) pour que, dans un premier temps, 
    l'adresse virtuelle 0x100000 soit égale à l'adresse physique 0x100000. Cela évite que le noyau ne plante instantanément à l'activation.

activer le Paging (la pagination).

Pourquoi ? Actuellement, ton OS accède directement à la RAM physique. Avec le paging, tu créeras une "mémoire virtuelle". C'est ce qui permet :

    De protéger le noyau (un programme ne peut pas écraser ton code).

    De faire croire à chaque programme qu'il est seul au monde à l'adresse 0x000000.

Est-ce que tu te sens prêt à t'attaquer à la configuration des registres du processeur (CR0, CR3) pour activer la pagination virtuelle ?
*/

#include "paging.h"

// On aligne les tables sur 4096 octets (obligatoire pour le CPU)
uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t second_page_table[1024] __attribute__((aligned(4096)));

void init_paging() {
    // 1. Remplir le Page Directory
    // Attribut 0x03 : Read/Write + Present
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Not present, RW
    }

    // 2. Remplir la première Page Table (couvre les 4 premiers Mo)
    for(unsigned int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 3; // Adresse physique | Present | RW
    }

    // 2.bis Remplir la DEUXIÈME Page Table (de 4 Mo à 8 Mo)
    for(unsigned int i = 0; i < 1024; i++) {
        // (i * 4096) + 4MB
        second_page_table[i] = (0x400000 + (i * 4096)) | 3; 
    }

    // 3. Mettre la table dans le répertoire
    page_directory[0] = ((unsigned int)first_page_table) | 3;
    // 3.bis Mettre la deuxième table dans le répertoire à l'index 1
    page_directory[1] = ((unsigned int)second_page_table) | 3;
}