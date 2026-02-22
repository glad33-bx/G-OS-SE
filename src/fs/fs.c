#include "fs.h"
/*#include "mem.h"
#include "terminal.h"
#include "string.h"
#include "gos_memory.h"
#include "config.h"*/
/*
Système de fichiers : Le RAMDisk

Étant donné que nous n'avons pas encore de pilote pour disque dur (IDE/SATA), 
la méthode standard est de créer un RAMDisk. C'est une zone de la mémoire RAM que l'on traite comme un disque.
*/

//struct file files[MAX_FILES];
vfile_t files[MAX_FILES];

unsigned char fs_data_pool[MAX_FILES][4096]; // Réserve 16 pages de 4ko

int init_fs() {  
    // On initialise les fichiers à "non utilisé"
    for (int i = 0; i < MAX_FILES; i++) {
        //files[i].used = 0;
        files[i].active = 0;
    }

    return 0; // On renvoie 0 pour dire "Tout est OK"
}

int create_file(char* name) {
    // 1. Validations de base (nom vide ou trop long)
    if (name == NULL || strlen(name) == 0) return -1;
    if (strlen(name) >= MAX_FILENAME_LEN) return -2;

    // 2. VÉRIFICATION D'UNICITÉ
    // Si find_file_idx ne renvoie pas -1, c'est que le nom existe déjà
    if (find_file_idx(name) != -1) {
        kprintf("FS: Erreur - Le fichier '%s' existe deja.\n", name);
        return -3; // Code erreur pour doublon
    }

    // 3. Recherche d'un slot libre
    int idx = get_free_slot();
    if (idx == -1) {
        kprintf("FS: Erreur - Table des fichiers pleine.\n");
        return -4;
    }

    // 4. Création effective
    strcpy(files[idx].name, name);
    files[idx].size = 0;
    files[idx].active = 1;
    files[idx].data = NULL;

    return 0; // Succès
}

// Trouve un emplacement vide dans la table des fichiers
int get_free_slot() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].active) return i;
    }
    return -1;
}

// Trouve l'index d'un fichier par son nom
int find_file_idx(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active) {
            // Debug: affiche le nom cherché et le nom trouvé entre crochets
            #if DEBUG
                kprintf("Comparing [%s] with [%s]\n", name, files[i].name);
            #endif
            if (strcmp(files[i].name, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

// Fonction pour la commande "ls"
void list_files() {
    kprintf("\n%v--- Index des fichiers ---%v\n", VGA_LIGHT_MAGENTA, VGA_WHITE);
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active) {
            kprintf(" - [%s] (%d octets)\n", files[i].name, files[i].size);
            count++;
        }
    }
    if (count == 0) kprintf("(Aucun fichier)\n");
}

// Fonction pour l'écriture (echo "texte" > fichier)
int write_file(char* name, char* content) {
    trim(name); // On nettoie le nom
    int idx = find_file_idx(name);
    
    if (idx == -1) {
        kprintf("FS: Erreur - Fichier [%s] introuvable\n", name);
        return -1;
    }

    int len = strlen(content);
    kprintf("FS: Ecriture dans %s (%d octets)\n", name, len);

    // Libération de l'ancienne mémoire
    if (files[idx].data != NULL) {
        kfree(files[idx].data);
        files[idx].data = NULL;
    }

    // Allocation
    void* new_ptr = kmalloc(len + 1);
    if (new_ptr == NULL) {
        kprintf("MEM: Erreur - Allocation de %d octets impossible\n", len + 1);
        return -1;
    }

    files[idx].data = (uint8_t*)new_ptr;
    strcpy((char*)files[idx].data, content);
    files[idx].size = len;
    
    kprintf("FS: Succes - Contenu ecrit\n");
    return 0;
}

// Au lieu d'accéder au tableau files depuis le clavier, crée une fonction dans fs.c qui renvoie les données,
uint8_t* get_file_data(int idx) {
    if (idx >= 0 && idx < MAX_FILES && files[idx].active) {
        return files[idx].data;
    }
    return NULL;
}

int is_valid_name(char* name) {
    while (*name) {
        if (*name == ';' || *name == '>' || *name == '<' || *name == '|') {
            return 0; // Caractère interdit
        }
        name++;
    }
    return 1;
}

void read_file(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        // On cherche le fichier actif avec le bon nom
        if (files[i].active && strcmp(files[i].name, name) == 0) {
            
            if (files[i].size == 0 || files[i].data == 0) {
                kprintf("\n(Fichier vide)\n");
                return;
            }

            // Affichage du contenu
            kprintf("\n"); // On passe à la ligne avant le contenu
            for (uint32_t j = 0; j < files[i].size; j++) {
                putc(files[i].data[j]);
            }
            kprintf("\n");
            return;
        }
    }
    kprintf("\nErreur : Fichier '%s' non trouve.\n", name);
}

int delete_file(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active && strcmp(files[i].name, name) == 0) {
            kfree(files[i].data); // On rend la mémoire au système !
            files[i].active = 0;  // Libère l'entrée dans l'index
            files[i].size = 0;
            files[i].data = 0;    // Le pointeur est oublié
            // Note: la mémoire dans le tas reste occupée (pas de kfree encore)
            return 0;
        }
    }
    return -1; // Fichier non trouvé
}

// Cherche un fichier commençant par 'prefix' et renvoie son nom complet
char* find_file_by_prefix(const char* prefix) {
    if (strlen(prefix) == 0) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active) {
            if (str_starts_with(files[i].name, prefix)) {
                return files[i].name;
            }
        }
    }
    return 0; // Aucun fichier trouvé
}



