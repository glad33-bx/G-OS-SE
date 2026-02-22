#include "../src/lib/ulib.h"

// En déclarant ces variables ici, on crée des symboles "Data"
char* msg1 = "Lancement unique !\n";
char* msg2 = "GillesOS App en C chargee !\n";
char* msg3 = "Argument recu du shell : ";
char* msg4 = "\n";
char* msg5 = "Aucun argument fourni.\n";

void main() {
    print(msg1);
    print_color(msg2, 0x0E); 

    char* arg;
    asm volatile("mov %%edx, %0" : "=r"(arg));

    if (arg && *arg != '\0') {
        print(msg3);
        print(arg);
        print(msg4);
    } else {
        print(msg5);
    }

    exit(0);
}