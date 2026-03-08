// #include "ulib.h"

// On définit kprintf pour satisfaire le linker des APPS
// Mais on le redirige vers la fonction d'affichage de l'application
void kprintf(const char* fmt, ...) {
    // Option A: Ne rien faire
    // Option B: Appeler ton syscall de print (si tu as un wrapper printf)
}