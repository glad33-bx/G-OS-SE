#include "ulib.h"
// #include "../src/lib/string.h"
#include "string.h"

extern char* strstr(const char* haystack, const char* needle);

/*void main() {
    char buffer[1024];
    // On appelle notre nouveau syscall
    // On peut passer "." pour le dossier courant si ton driver le gère
    if (sys_ls(".", buffer) == 0) {
        printf("%s\n", buffer);
    } else {
        printf("Erreur lors de la lecture du repertoire.\n");
    }
    exit(0);
}*/

/*void main() {
    char full_cmd[128];
    char list_buffer[2048];
    
    // 1. Récupérer la commande via le syscall
    get_command_line(full_cmd);

    // 2. Chercher si "-l" est présent dans la commande
    // On passe l'argument au syscall ls
    if (strstr(full_cmd, "-l")) {
        sys_ls("-l", list_buffer);
    } else {
        sys_ls(".", list_buffer);
    }

    printf(list_buffer);
    exit(0);
}*/

void main() {
    char full_cmd[128];
    char list_buffer[2048];
    get_command_line(full_cmd);

    sys_ls(strstr(full_cmd, "-l") ? "-l" : ".", list_buffer);

    // On découpe le résultat ligne par ligne
    char* line = strtok(list_buffer, "\n");
    while (line != NULL) {
        if (strstr(line, "<DIR>")) {
            sys_set_color(0x09, 0x00); // Bleu sur Noir
        } else if (strstr(line, ".BIN")) {
            sys_set_color(0x0A, 0x00); // Vert sur Noir
        } else if (strstr(line, "---") || strstr(line, "Nom")) {
            sys_set_color(0x0F, 0x00); // Blanc brillant pour l'en-tête
        } else {
            sys_set_color(0x07, 0x00); // Gris clair pour le reste
        }

        printf(line);
        printf("\n");
        line = strtok(NULL, "\n");
    }

    sys_set_color(0x07, 0x00); // Reset final
    exit(0);
}