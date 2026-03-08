// cat.c (Application)
#include "../src/lib/ulib.h"
#include "../src/lib/string.h"

void main() {
    char cmd[128];
    char file_buffer[2048]; // Buffer pour stocker le contenu lu
    get_command_line(cmd); 

    // Extraction de l'argument (le nom du fichier après l'espace)
    char* filename = "AUCUN";
    for(int i = 0; cmd[i] != '\0'; i++) {
        if(cmd[i] == ' ') {
            // On saute tous les espaces
            while(cmd[i] == ' ') i++;
            if(cmd[i] != '\0') {
                filename = &cmd[i];
                break;
            }
        }
    }

    if (strcmp(filename, "AUCUN") == 0) {
        printf("Usage: cat <fichier>\n");
    } else {
        printf("Lecture de : %s\n", filename);
        
        // Appel au syscall que nous avons créé précédemment
        // On demande au kernel de remplir file_buffer
        int bytes_read = sys_read_file(filename, file_buffer, 2047);

        if (bytes_read > 0) {
            file_buffer[bytes_read] = '\0'; // Assure la fin de chaîne pour printf
            printf("--------------------------\n");
            printf("%s\n", file_buffer);
            printf("--------------------------\n");
        } else {
            printf("Erreur : Impossible de lire le fichier (ou fichier vide).\n");
        }
    }

    exit(0); 
}