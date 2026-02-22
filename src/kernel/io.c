#include "io.h"

/*
Le moyen le plus simple de redémarrer un PC x86 sans passer par des protocoles complexes
est de demander au contrôleur clavier (port 0x64) de réinitialiser le CPU.
*/
void reboot() {
    kprintf("\nRedemarrage en cours...");

    // Le "Fast Reset" via le contrôleur clavier (PS/2)
    // On attend que le contrôleur soit prêt
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    // On envoie la commande de reset (0xFE)
    outb(0x64, 0xFE);
    
    // Si ça échoue (cas rare en MV), on bloque
    while(1); 
}



