#ifndef TIMER_H
#define TIMER_H

#include "../lib/gos_types.h"
#include "terminal.h"
#include "timer.h"
#include "idt.h"
#include "rtc.h"  // Pour display_clock()

/**
 * Initialise le PIT (Programmable Interval Timer) à une fréquence donnée.
 * Par défaut, on utilise environ 100Hz ou la fréquence native de 18.2Hz.
 */
void init_timer(uint32_t frequency);

/**
 * Handler appelé par l'interruption IRQ0 (0x20).
 * C'est ici que l'on incrémente les ticks et qu'on appelle l'horloge.
 */
void timer_handler();

/**
 * Permet de faire une pause de 'ticks' millisecondes/cycles.
 */
void sleep(uint32_t ticks);

// Variable globale pour compter le temps depuis le boot
extern unsigned long timer_ticks;

#endif