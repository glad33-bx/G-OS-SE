#include "../src/lib/ulib.h"

void main() {
    printf("Tapez des touches ( 'q' pour quitter ) :\n");

    while(1) {
        char c = getc();
        if (c == 'q') break;
        
        printf("Tu as tape : %c\n", c);
    }

    printf("\nFin du programme.\n");
    exit(0);
}