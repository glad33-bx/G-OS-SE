/*
Le "Beep" système est un vestige fantastique de l'informatique classique. Contrairement à une carte son moderne, le PC Speaker est piloté directement par le PIT (Programmable Interval Timer) à l'adresse 0x43 et 0x42, couplé au port système 0x61.

Voici comment donner de la voix à ton noyau.
1. La logique technique

Le haut-parleur est relié au canal 2 du PIT. Pour faire un son, on doit :

    Configurer le PIT pour générer une onde carrée.

    Calculer le diviseur de fréquence (la fréquence d'entrée est de 1.193182 MHz).

    Connecter le PIT au haut-parleur via le port 0x61.
*/

#include "gos_types.h"
#include "pit.h"
#include "io.h"
#include "timer.h"

extern  uint32_t beep_countdown;

// Joue un son à une fréquence donnée
void play_sound(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;

    // Calcul du diviseur pour le PIT
    Div = 1193180 / nFrequence;

    // Configurer le PIT : Canal 2, accès LSB/MSB, mode 3 (onde carrée), binaire
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t) (Div));
    outb(0x42, (uint8_t) (Div >> 8));

    // Récupérer l'état du port 0x61 et activer les bits 0 et 1
    // Bit 0 : Connecte le PIT au HP / Bit 1 : Active le HP
    tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}


// Le haut-parleur du PC (PC Speaker) est contrôlé par le port 0x61 (System Control Port B). 
// Pour couper le son, il faut forcer les deux premiers bits de ce port à 0
void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC; // Désactive les bits 0 et 1
    
    // On force les bits 0 et 1 à 0
    // Bit 0 : Connecte le Timer 2 au haut-parleur
    // Bit 1 : Active/Désactive le haut-parleur lui-même
    outb(0x61, tmp);
}

void beep() {
    play_sound(200); // Fréquence 750Hz
    sleep(10);       // Bruit court de 50ms
    nosound();
}

// Attend un certain nombre de millisecondes
// (Nécessite que init_timer(100) soit appelé pour avoir 1 tick = 10ms)
void sleep(uint32_t ms) {
    // Si ton timer est à 100Hz, 1 tick = 10ms
    // On calcule le nombre de ticks à attendre
    unsigned long end_ticks = timer_ticks + (ms / 10);
    while (timer_ticks < end_ticks) {
        __asm__ __volatile__("hlt"); // Économise le CPU en attendant l'interruption
    }
}

void beep_async(uint32_t duration_ms) {
    play_sound(750);
    // Si ton timer est à 100Hz, 1 tick = 10ms
    beep_countdown = duration_ms / 10; 
}