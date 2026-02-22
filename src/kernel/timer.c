#include "io.h"
#include "timer.h"
#include "pit.h"

volatile uint32_t beep_countdown = 0;

unsigned long timer_ticks = 0;

/*
Le Timer envoie une interruption (IRQ 0) à intervalles réguliers. 
Par défaut, c'est environ 18,2 fois par seconde.
*/

/*
+ wrapper dans src/interrupt.asm
*/
void timer_handler() {
    timer_ticks++;
    
    // Si un décompte de bip est actif
    if (beep_countdown > 0) {
        beep_countdown--;
        if (beep_countdown == 0) {
            nosound(); // Le timer coupe le son automatiquement !
        }
    }
    outb(0x20, 0x20);// EOI
}

/**
 * Initialise le timer système (PIT)
 * frequency : nombre d'interruptions par seconde (ex: 100 pour 100Hz)
 */
void init_timer(uint32_t frequency) {
    // 1. Calculer le diviseur
    // Le PIT tourne à 1 193 182 Hz. 
    uint32_t divisor = 1193182 / frequency;

    // 2. Envoyer la commande de contrôle (0x36)
    // 0x36 = 00110110 en binaire :
    // Canal 0, accès bas/haut, mode 3 (générateur d'onde carrée), binaire.
    outb(0x43, 0x36);

    // 3. Envoyer le diviseur (doit être envoyé en deux fois sur le port 0x40)
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, low);
    outb(0x40, high);
}


