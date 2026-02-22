#include "fat.h"
#include "ata.h"
#include "../kernel/gos_memory.h"
#include "../kernel/terminal.h"
#include "../lib/string.h"
#include "../kernel/time.h"
#include "../kernel/rtc.h"
#include "../shell/shell.h"

uint32_t fs_status=false;

//struct fat_bpb bpb;
struct fat32_bpb bpb; // L'instance réelle qui contient les données du disque

// 0 représente généralement le répertoire racine (Root) en FAT16
uint32_t current_dir_cluster = 0;

// On isole totalement le buffer dans sa propre page mémoire
uint8_t fat_tmp_buffer[512] __attribute__((aligned(4096)));
// Un deuxième buffer global pour éviter les conflits
uint32_t fat_search_buffer[256] __attribute__((aligned(4096)));

static uint32_t root_dir_sector;
static uint32_t data_sector;


// Déclaration (pour que fat.c sache qu'elle existe)
void get_current_datetime(int *day, int *month, int *year, int *hour, int *min, int *sec);

// Variables globales à utiliser partout
uint32_t fat_lba;           // Début de la FAT1
uint32_t data_lba;          // Début de la zone de données
uint32_t root_cluster;      // Cluster de départ (souvent 2)
uint8_t  sectors_per_cluster;
uint32_t sectors_per_fat;   // Taille d'une FAT en secteurs

uint32_t cluster_to_lba(uint32_t cluster) {
    // Calcul de la taille du répertoire racine en secteurs
    uint32_t root_dir_sectors = (bpb.root_dir_entries * 32) / bpb.bytes_per_sector;
    
    // Calcul du premier secteur de données (Data Region)
    uint32_t first_data_sector = bpb.reserved_sectors + (bpb.fats * bpb.sectors_per_fat32) + root_dir_sectors;
    
    // Formule FAT : Le cluster 2 est le premier cluster de données
    return first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

// Cette fonction remplace ton ancienne logique de recherche
// Utilise le secteur calculé dynamiquement !
uint32_t fat_find_file_cluster(const char* name_to_find) {
    // Note: Utilise un buffer assez grand pour un cluster complet
    uint8_t cluster_buf[4096]; 
    uint32_t current_cluster = root_cluster;

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) return -1;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; // FIX ICI
            if (entries[i].attributes == 0x0F) continue;

            if (memcmp(entries[i].name, name_to_find, 11) == 0) {
                return ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
            }
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    return -1;
}

// Cette fonction écrit dans la table FAT elle-même pour dire "ce cluster est occupé".
/*void fat_set_cluster_value(uint32_t cluster, uint32_t value) {
    if (cluster < 2) return; // Sécurité ultime

    uint32_t fat_offset = cluster * 2;
    uint32_t sector_dans_fat = fat_offset / 512;
    uint32_t octet_dans_secteur = fat_offset % 512;
    uint32_t lba_absolu = bpb.reserved_sectors + sector_dans_fat;

    // Utilisation du buffer global pour éviter de faire exploser la pile
    asm volatile("cli");
    ata_read_sector(lba_absolu, (uint32_t*)fat_tmp_buffer);

    // Injection Little Endian
    fat_tmp_buffer[octet_dans_secteur] = (uint8_t)(value & 0xFF);
    fat_tmp_buffer[octet_dans_secteur + 1] = (uint8_t)((value >> 8) & 0xFF);

    ata_write_sector(lba_absolu, (uint32_t*)fat_tmp_buffer); // FAT 1
    ata_write_sector(lba_absolu + bpb.sectors_per_fat, (uint32_t*)fat_tmp_buffer); // FAT 2
    
    // OPTIONNEL : Si bpb.fats > 1, il faudrait aussi écrire dans bpb.reserved_sectors + bpb.sectors_per_fat + sector_dans_fat
    asm volatile("sti");
}
*/
// En FAT32, un cluster contient plusieurs secteurs. 
// Il faut une fonction pour lire un cluster entier dans un buffer.
void fat_read_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = data_lba + ((cluster - 2) * sectors_per_cluster);
    for (uint8_t i = 0; i < sectors_per_cluster; i++) {
        ata_read_sector(lba + i, (uint16_t*)(buffer + (i * 512)));
    }
}

void fat_write_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = data_lba + ((cluster - 2) * sectors_per_cluster);
    for (uint8_t i = 0; i < sectors_per_cluster; i++) {
        ata_write_sector(lba + i, (uint16_t*)(buffer + (i * 512)));
    }
}
// Elle cherche les 32 octets d'une entrée par son nom. 
// C'est elle qui permet de lire les attributs (est-ce un dossier ?).
void fat_create_entry(char* name, uint8_t attr, uint32_t cluster) {
    char fat_name[11];
    to_fat_name(name, fat_name); // Convertit "mon_dir" en "MON_DIR    "

    //uint8_t buffer[512];
    //uint8_t*)fat_tmp_buffer;
    uint32_t sector = (current_dir_cluster == 0) ? root_dir_sector : cluster_to_lba(current_dir_cluster);
    
    ata_read_sector(sector, (uint16_t*)fat_tmp_buffer);

    for (int i = 0; i < 512; i += 32) {
        // 0x00 = Entrée libre, 0xE5 = Entrée supprimée (réutilisable)
        if (fat_tmp_buffer[i] == 0x00 || (uint8_t)fat_tmp_buffer[i] == 0xE5) {
            memcpy(&fat_tmp_buffer[i], fat_name, 11); // Nom
            fat_tmp_buffer[i + 11] = attr;           // Attribut (0x10 pour dossier)
            
            // On met à jour le cluster de départ (octets 26-27)
            fat_tmp_buffer[i + 26] = cluster & 0xFF;
            fat_tmp_buffer[i + 27] = (cluster >> 8) & 0xFF;

            // Taille à 0 pour un dossier (octets 28-31)
            *(uint32_t*)&fat_tmp_buffer[i + 28] = 0;

            //ata_write_sector(sector, buffer);
            ata_write_sector(sector, (uint16_t*)fat_tmp_buffer);
            return;
        }
    }
    kprintf("Erreur : Repertoire plein !\n");
}

/*
Un nouveau dossier n'est pas vide : il doit contenir . (lui-même) et .. (son parent). 
C'est ce qui permet au système de faire cd ...
*/
void fat_init_sub_directory(uint32_t new_cluster, uint32_t parent_cluster) {
    //uint8_t buffer[512] = {0}; // Secteur vide
    //(uint8_t*)fat_tmp_buffer;
    // Entrée "." (Lui-même)
    memcpy(&fat_tmp_buffer, ".          ", 11);
    fat_tmp_buffer[11] = 0x10; // Attribut dossier
    fat_tmp_buffer[26] = new_cluster & 0xFF;
    fat_tmp_buffer[27] = (new_cluster >> 8) & 0xFF;

    // Entrée ".." (Le parent)
    memcpy(&fat_tmp_buffer[32], "..         ", 11);
    fat_tmp_buffer[32+11] = 0x10;
    fat_tmp_buffer[32+26] = parent_cluster & 0xFF;
    fat_tmp_buffer[32+27] = (parent_cluster >> 8) & 0xFF;

    // On écrit ce premier secteur du nouveau dossier
    //ata_write_sector(cluster_to_lba(new_cluster), buffer);
    ata_write_sector(fat_cluster_to_lba(new_cluster), (uint16_t*)fat_tmp_buffer);
}

// fat_init() qui lit le secteur 0 et vérifie si bytes_per_sector vaut bien 512.
/*void fat_init() {
    uint8_t sector0[512];
    ata_read_sector(0, (uint32_t*)sector0);

    // Initialise tes variables globales ici...
    fat_lba = *(uint32_t*)&sector0[14];
    sectors_per_cluster = sector0[13];
    uint32_t spf32 = *(uint32_t*)&sector0[36];
    data_lba = fat_lba + (sector0[16] * spf32);
    root_cluster = *(uint32_t*)&sector0[44];
}*/

void fat_init() {
    uint8_t boot_sector[512];
    
    // 1. Lire le secteur 0 (Boot Sector / BPB)
    ata_read_sector(0, (uint16_t*)boot_sector);
    struct fat32_bpb* bpb32 = (struct fat32_bpb*)boot_sector;

    // 2. Vérifier la signature pour être sûr (Optionnel)
    if (bpb32->bytes_per_sector != 512) {
        kprintf("[FAT] Erreur: Secteur non supporte (%d octets)\n", bpb32->bytes_per_sector);
        return;
    }

    // 3. Extraire les paramètres vitaux
    sectors_per_cluster = bpb32->sectors_per_cluster;
    root_cluster = bpb32->root_cluster;
    sectors_per_fat = bpb32->sectors_per_fat32;

    // 4. Calculer les adresses LBA
    // La FAT commence juste après les secteurs réservés
    fat_lba = bpb32->reserved_sectors;

    // La zone de données commence après les secteurs réservés + toutes les FATs
    // En FAT32, le répertoire racine est DANS la zone de données (cluster 2 par défaut)
    data_lba = bpb32->reserved_sectors + (bpb32->fats * sectors_per_fat);

    fs_status = true;
    
    kprintf("[FAT] Initialise: FAT_LBA=%d, DATA_LBA=%d, RootCluster=%d\n", 
            fat_lba, data_lba, root_cluster);
}

// lister les fichiers du disque. Secteur 132
/*void fat_list_root() {
    // Plus besoin du check NULL ici pour un buffer statique
    //ata_read_sector(132, (uint32_t*)fat_tmp_buffer);
    ata_read_sector(root_dir_sector, (uint32_t*)fat_tmp_buffer);

    struct fat_directory_entry* entry = (struct fat_directory_entry*)fat_tmp_buffer;
    
    // On vérifie s'il y a au moins un fichier
    //if (entry->name[0] != 0 && entry->name[0] != 0xE5) {
    if ( (uint8_t)entry->name[0] != 0xE5 ){
        #if DEBUG
            kprintf("Fichier trouve: ");
        #endif
         for(int i=0; i<8; i++) putc(entry->name[i]);
         kprintf("\n");
    } else {
        kprintf("Aucun fichier valide dans le premier slot.\n");
    }
}*/

void fat_ls() {
    uint8_t cluster_buf[4096]; // Ajuste selon ton sectors_per_cluster (ex: 8 * 512)
    uint32_t current_cluster = root_cluster;

    kprintf("Listing de / :\n");

    while (current_cluster < 0x0FFFFFF8) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;

        // 16 entrées par secteur * sectors_per_cluster
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            //if (entries[i].name[0] == 0x00) return;
            if ((uint8_t)entries[i].name[0] == 0x00) return; // Fin totale
            //if (entries[i].name[0] == 0xE5) continue;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; // Supprimé
            if (entries[i].attributes == 0x0F) continue; // Nom long (LFN)

            // Formater le nom pour l'affichage
            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
            if(entries[i].attributes != 0x10) { // Si ce n'est pas un dossier, on ajoute l'extension
                clean_name[p++] = '.';
                for(int j=8; j<11; j++) if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
            }
            clean_name[p] = '\0';

            kprintf("  %s  (%d octets)  Cluster: %d\n", clean_name, entries[i].size, 
                    ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low);
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }
}

// Cette fonction va lire la table FAT (qui commence au secteur 4 sur ton disque) et chercher la première case vide.

/*uint32_t fat_find_free_cluster() {
    uint8_t fat_buffer[512];
    // On parcourt les secteurs de la FAT
    for (uint32_t s = 0; s < bpb.sectors_per_fat; s++) {
        ata_read_sector(bpb.reserved_sectors + s, (uint32_t*)fat_buffer);
        for (int i = 0; i < 512; i += 2) {
            uint32_t val = fat_buffer[i] | (fat_buffer[i+1] << 8);
            
            // Calculer l'index du cluster actuel
            uint32_t current_cluster = (s * 512 + i) / 2;
            
            if (val == 0x0000 && current_cluster >= 2) {
                return (uint32_t)current_cluster;
            }
        }
    }
    return 0; // Disque plein
}
*/
uint32_t fat_find_free_cluster() {
    uint8_t fat_table[512];
    // On commence à 2 car les clusters 0 et 1 sont réservés
    for (uint32_t i = 2; i < 262144; i++) { // 128 Mo / 512 octets (simplifié)
        uint32_t fat_sector = fat_lba + ((i * 4) / 512);
        uint32_t ent_offset = (i * 4) % 512;

        ata_read_sector(fat_sector, (uint16_t*)fat_table);
        uint32_t entry = *(uint16_t*)&fat_table[ent_offset] & 0x0FFFFFFF;

        if (entry == 0x0000000) {
            return i;
        }
    }
    return 0; // Disque plein
}

void fat_set_cluster_value(uint32_t cluster, uint32_t value) {
    uint8_t fat_table[512];
    uint32_t fat_sector = fat_lba + ((cluster * 4) / 512);
    uint32_t ent_offset = (cluster * 4) % 512;

    // 1. Lire le secteur de la FAT
    ata_read_sector(fat_sector, (uint16_t*)fat_table);
    
    // 2. Modifier la valeur (en gardant les 4 bits de poids fort réservés)
    uint32_t* entry_ptr = (uint32_t*)&fat_table[ent_offset];
    *entry_ptr = (*entry_ptr & 0xF0000000) | (value & 0x0FFFFFFF);
    
    // 3. Ré-écrire le secteur sur le disque
    ata_write_sector(fat_sector, (uint16_t*)fat_table);
    
    // 4. (Optionnel) En FAT, il y a souvent deux copies de la FAT
    // Il faudrait idéalement mettre à jour la FAT2 aussi (fat_sector + spf32)
}

/*void fat_set_cluster_value(uint32_t cluster, uint32_t value) {
    uint8_t fat_table[512];
    // spf32 doit être ta variable globale initialisée dans fat_init (ton 1015)
    uint32_t fat_sector = fat_lba + ((cluster * 4) / 512);
    uint32_t ent_offset = (cluster * 4) % 512;

    // --- Mise à jour FAT1 ---
    ata_read_sector(fat_sector, (uint32_t*)fat_table);
    uint32_t* entry_ptr = (uint32_t*)&fat_table[ent_offset];
    *entry_ptr = (*entry_ptr & 0xF0000000) | (value & 0x0FFFFFFF);
    ata_write_sector(fat_sector, (uint32_t*)fat_table);

    // --- Mise à jour FAT2 (La sauvegarde) ---
    // On ajoute 'spf32' au secteur pour atteindre la deuxième table
    ata_write_sector(fat_sector + spf32, (uint32_t*)fat_table);
}*/

/*
pour libérer une chaîne de clusters

On va créer une fonction dédiée qui suit la chaîne dans la FAT et remet chaque maillon à 0x0000
*/
/*void fat_free_cluster_chain(uint32_t start_cluster) {
    uint32_t current = start_cluster;
    uint32_t next;

    while (current >= 2 && current < 0xFFF8) {
        // 1. Lire quelle est la prochaine destination dans la FAT
        // On utilise ta logique de lecture de FAT
        uint32_t fat_offset = current * 4; // * 2;
        uint32_t sector_dans_fat = fat_offset / 512;
        uint32_t octet_dans_secteur = fat_offset % 512;
        uint32_t lba_absolu = bpb.reserved_sectors + sector_dans_fat;

        static uint8_t fat_buf[512] __attribute__((aligned(4096)));
        ata_read_sector(lba_absolu, (uint32_t*)fat_buf);

        // Récupérer la valeur du cluster suivant
        //next = fat_buf[octet_dans_secteur] | (fat_buf[octet_dans_secteur + 1] << 8);
        next = *(uint32_t*)&fat_buf[octet_dans_secteur] & 0x0FFFFFFF;
        // 2. Libérer le cluster actuel (mettre à 0x0000)
        fat_set_cluster_value(current, 0x0000);

        #if DEBUG
        kprintf("[FS] Cluster %d libere.\n", current);
        #endif

        // 3. Passer au suivant
        current = next;
    }
}*/

/*void fat_free_cluster_chain(uint32_t start_cluster) {
    uint32_t current = start_cluster;
    uint32_t next;

    while (current >= 2 && current < 0x0FFFFFF8) {
        // 1. Trouver le secteur et l'offset pour un cluster de 4 octets (FAT32)
        uint32_t fat_offset = current * 4; 
        uint32_t fat_sector = fat_lba + (fat_offset / 512);
        uint32_t ent_offset = fat_offset % 512;

        static uint8_t fat_buf[512] __attribute__((aligned(4096)));
        ata_read_sector(fat_sector, (uint16_t*)fat_buf);

        // 2. Récupérer le cluster suivant (32 bits)
        next = *(uint32_t*)&fat_buf[ent_offset] & 0x0FFFFFFF;

        // 3. Libérer le cluster actuel
        fat_set_cluster_value(current, 0x00000000);

        current = next;
    }
}*/

void fat_free_cluster_chain(uint32_t cluster) {
    uint32_t current = cluster;
    while (current < 0x0FFFFFF8 && current >= 2) {
        uint32_t next = fat_get_next_cluster(current);
        fat_set_cluster_value(current, 0); // Libère le cluster actuel
        current = next;
    }
}

/*uint32_t fat_cluster_to_lba(uint32_t cluster) {
    return data_sector + (cluster - 2) * bpb.sectors_per_cluster;
}*/

uint32_t fat_cluster_to_lba(uint32_t cluster) {
    return data_lba + (cluster - 2) * sectors_per_cluster;
}


// lire la FAT sans écrire dedans
uint32_t fat_get_next_cluster(uint32_t cluster) {
    uint8_t fat_table[512];
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    ata_read_sector(fat_sector, (uint16_t*)fat_table);
    uint32_t next_cluster = *(uint32_t*)&fat_table[ent_offset];
    return next_cluster & 0x0FFFFFFF;
}

/*struct fat_directory_entry* fat_find_file(const char* name) {
    // On lit le secteur du répertoire racine (132 selon tes logs)
    //ata_read_sector(132, (uint32_t*)fat_tmp_buffer);
    ata_read_sector(root_dir_sector, (uint32_t*)fat_tmp_buffer);

    struct fat_directory_entry* entries = (struct fat_directory_entry*)fat_tmp_buffer;
    
    // On parcourt les 16 entrées possibles dans un secteur de 512 octets
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0) break; // Fin du répertoire
        
        // Comparaison simplifiée (on compare les 8 premiers caractères)
        if (memcmp(entries[i].name, name, 8) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}*/

/*gère le rembourrage d'espaces 
n FAT, les noms de fichiers sont un peu particuliers : ils doivent toujours faire 11 caractères 
(8 pour le nom, 3 pour l'extension) complétés par des espaces.

Par exemple, "TEST.TXT" doit devenir "TEST TXT".*/

/*void to_fat_name(const char* input, char* output) {
    for (int i = 0; i < 11; i++) output[i] = ' ';

    int i = 0, j = 0;
    // Nom principal
    while (input[i] != '.' && input[i] != '\0' && j < 8) {
        output[j++] = toupper(input[i++]);
    }
    // Extension
    if (input[i] == '.') {
        i++; 
        j = 8;
        while (input[i] != '\0' && j < 11) {
            output[j++] = toupper(input[i++]);
        }
    }
}*/

void to_fat_name(const char* input, char* output) {
    memset(output, ' ', 11); // Remplir d'espaces
    int i = 0, j = 0;

    // Nom principal (8 char max)
    while (input[i] && input[i] != '.' && j < 8) {
        if (input[i] >= 'a' && input[i] <= 'z') 
            output[j++] = input[i++] - 32; // Uppercase
        else 
            output[j++] = input[i++];
    }

    // On avance jusqu'au point
    while (input[i] && input[i] != '.') i++;
    if (input[i] == '.') i++;

    // Extension (3 char max)
    j = 8;
    while (input[i] && j < 11) {
        if (input[i] >= 'a' && input[i] <= 'z') 
            output[j++] = input[i++] - 32;
        else 
            output[j++] = input[i++];
    }
}

// lit et affiche cluster par cluster
void fat_cat(const char* filename) {
    #if DEBUG
        kprintf("[DEBUG] cat cherche %s. Root Cluster: %d, Data LBA: %d\n", filename, root_cluster, data_lba);
    #endif
    char fat_name[11];
    to_fat_name(filename, fat_name); // "notes.txt" -> "NOTES   TXT"
    // 1. Trouver le fichier et récupérer son entrée complète
    // On a besoin de la taille exacte
    uint8_t cluster_buf[4096]; 
    uint32_t current_cluster = root_cluster;
    struct fat_dir_entry file_entry;
    int found = 0;

    // --- Recherche du fichier ---
    /*while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        for (int i = 0; i < (sectors_per_cluster * 512) / 32; i++) {
            kprintf("Comparaison: [%s] vs [%s] - fat name : %s\n", entries[i].name, filename,fat_name);
            if (entries[i].name[0] == 0) break;
            // On compare avec le nom formaté (8.3)
            //if (memcmp(entries[i].name, filename, 11) == 0) {
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                file_entry = entries[i];
                found = 1;
                break;
            }
        }
        if (found) break;
        current_cluster = fat_get_next_cluster(current_cluster);
    }*/

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        
        // Calcule le nombre d'entrées (souvent 128 pour un cluster de 4ko)
        int entries_per_cluster = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < entries_per_cluster; i++) {
            // Si le premier octet est 0, c'est la fin du répertoire
            if (entries[i].name[0] == 0) {
                // kprintf("Fin de repertoire atteinte a l'entree %d\n", i);
                break; 
            }

            // On affiche enfin ce qu'on lit
            // Attention: name n'est pas une chaine terminée par \0, 
            // kprintf %s risque de lire trop loin si tu n'utilises pas une limite.
            // Utilise plutot une boucle pour afficher les 11 caracteres.
            
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                file_entry = entries[i];
                found = 1;
                break;
            }
        }
        if (found) break;
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    if (!found) {
        kprintf("Fichier non trouve.\n");
        return;
    }

    // 2. Lecture du contenu cluster par cluster
    uint32_t size_left = file_entry.size;
    uint32_t file_cluster = ((uint32_t)file_entry.cluster_high << 16) | file_entry.cluster_low;

    while (size_left > 0 && file_cluster < 0x0FFFFFF8 && file_cluster >= 2) {
        fat_read_cluster(file_cluster, cluster_buf);
        
        // Calculer combien d'octets afficher dans ce cluster
        uint32_t to_read = (sectors_per_cluster * 512);
        if (size_left < to_read) to_read = size_left;

        // Afficher les octets
        for (uint32_t i = 0; i < to_read; i++) {
            kprintf("%c", cluster_buf[i]);
        }

        size_left -= to_read;
        if (size_left > 0) {
            file_cluster = fat_get_next_cluster(file_cluster);
        }
    }
    kprintf("\n");
}

// Dans fat.h, ajoute la taille à la structure de retour ou crée une struct spécifique
// Pour faire simple, on va modifier fat_get_file_content pour ajouter un \0
void* fat_get_file_content(uint32_t cluster) {
    static uint8_t file_buffer[8192]; // Buffer statique (pour test)
    uint32_t curr = cluster;
    uint32_t offset = 0;

    while (curr < 0x0FFFFFF8 && curr >= 2) {
        // Lire le cluster actuel dans le buffer à l'offset courant
        fat_read_cluster(curr, file_buffer + offset);
        
        offset += (sectors_per_cluster * 512);
        if (offset >= 8192) break; // Sécurité buffer

        curr = fat_get_next_cluster(curr);
    }
    return (void*)file_buffer;
}

// ette fonction doit aller chercher l'entrée du fichier dans le répertoire (Root ou sous-dossier) 
// et mettre à jour les 4 octets de la taille (offset 28 de l'entrée FAT).
void fat_update_file_size(const char* filename, uint32_t new_size) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // On détermine le secteur du répertoire actuel
    uint32_t sector = (current_dir_cluster == 0) ? root_dir_sector : cluster_to_lba(current_dir_cluster);

    static uint8_t dir_buf[512] __attribute__((aligned(4096)));
    
    asm volatile("cli");
    ata_read_sector(sector, (uint16_t*)dir_buf);
    asm volatile("sti");

    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;

    for (int i = 0; i < 16; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            // Mise à jour de la taille
            entries[i].size = new_size;

            // Optionnel : On en profite pour mettre à jour la date de modification
            uint32_t ts = get_fat_timestamp();
            entries[i].last_mod_time = (uint32_t)(ts & 0xFFFF);
            entries[i].last_mod_date = (uint32_t)(ts >> 16);

            // On réécrit le secteur sur le disque
            asm volatile("cli");
            ata_write_sector(sector, (uint16_t*)dir_buf);
            asm volatile("sti");
            return;
        }
    }
}

/*
fonction "helper"
mettre à jour fat_create_file (pour la date de création) et fat_overwrite_file_content (pour la date de modification).
*/
uint32_t get_fat_timestamp() {
    int day, month, year, hour, min, sec;
    
    // Appel de la fonction qu'on vient de créer
    get_current_datetime(&day, &month, &year, &hour, &min, &sec);

    // Encodage au format FAT (Bit-field)
    uint32_t fat_date = (uint32_t)((((year - 1980) & 0x7F) << 9) | ((month & 0x0F) << 5) | (day & 0x1F));
    uint32_t fat_time = (uint32_t)(((hour & 0x1F) << 11) | ((min & 0x3F) << 5) | ((sec / 2) & 0x1F));

    return (uint32_t)((fat_date << 16) | fat_time);
}

// Utilitaire pour comparer le nom "config.cnf" avec "CONFIG  CNF"
int fat_compare_name(const char* input, const char* fat_name) {
    char dest[11];
    memset(dest, ' ', 11);
    int i = 0, j = 0;

    // Convertir "config.cnf" -> "CONFIG  CNF"
    while (input[i] && input[i] != '.' && j < 8) {
        dest[j++] = toupper(input[i++]);
    }
    if (input[i] == '.') i++;
    j = 8;
    while (input[i] && j < 11) {
        dest[j++] = toupper(input[i++]);
    }

    return memcmp(dest, fat_name, 11) == 0;
}

void fat_overwrite_file_content(const char* filename, const char* new_text) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    uint32_t cluster = fat_find_file_cluster(fat_name);
    if (cluster == (uint32_t)-1) {
        kprintf("Erreur : Fichier introuvable.\n");
        return;
    }
    int first_cluster = fat_find_file_cluster(filename);
    if (first_cluster < 2) return;

    int total_len = strlen(new_text);
    int remaining_len = total_len;
    int current_cluster = first_cluster;
    int text_offset = 0;

    static uint8_t block_buffer[512] __attribute__((aligned(4096)));


    while (remaining_len > 0) {
        // 1. Préparer le buffer de 512 octets
        for(int i = 0; i < 512; i++) block_buffer[i] = 0;
        
        int to_write = (remaining_len > 512) ? 512 : remaining_len;
        memcpy(block_buffer, new_text + text_offset, to_write);

        // 2. Écrire le cluster actuel sur le disque
        uint32_t lba = data_sector + (current_cluster - 2) * sectors_per_cluster;
        asm volatile("cli");
        ata_write_sector(lba, (uint16_t*)block_buffer);
        asm volatile("sti");

        remaining_len -= to_write;
        text_offset += to_write;

        // 3. Si on a encore du texte, il nous faut un autre cluster
        if (remaining_len > 0) {
            // Est-ce qu'il y a déjà un cluster suivant dans la FAT ?
            uint32_t next = fat_get_next_cluster(current_cluster);
            
            if (next >= 0xFFF8) { // Fin de chaîne, on doit allouer !
                next = fat_find_free_cluster();
                if (next == 0) {
                    kprintf("Erreur : Disque plein, écriture tronquée.\n");
                    break;
                }
                // On lie l'ancien cluster au nouveau
                fat_set_cluster_value(current_cluster, next);
                // On marque le nouveau comme "Fin de fichier" (provisoirement)
                fat_set_cluster_value(next, 0xFFFF);
            }
            current_cluster = next;
        } else {
            // On a fini d'écrire : on s'assure que la chaîne s'arrête ici
            // On pourrait libérer les clusters restants si le nouveau texte est plus court
            // (optionnel pour une v1)
            fat_set_cluster_value(current_cluster, 0xFFFF);
        }
    }

    // 4. Mettre à jour la taille totale dans l'entrée de répertoire
    fat_update_file_size(filename, total_len);
}

/*void fat_overwrite_file(const char* filename, const char* text) {
    char fat_name[11];
    to_fat_name(filename, fat_name);
    
    // 1. Trouver l'entrée du fichier
    // (Utilise une version modifiée de ta boucle de recherche pour récupérer l'index)
    // ...
    
    // 2. Allouer un cluster si le fichier est vide
    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) return;

    // 3. Marquer comme occupé dans la FAT
    fat_set_cluster_value(new_cluster, 0x0FFFFFFF);

    // 4. Écrire le texte dans le cluster
    uint8_t data[4096]; // Taille d'un cluster
    memset(data, 0, 4096);
    strncpy((char*)data, text, 4095);
    fat_write_cluster(new_cluster, data);

    // 5. Mettre à jour l'entrée du répertoire (Cluster et Taille)
    // (C'est l'étape cruciale : lier le cluster à l'entrée trouvée à l'étape 1)
}*/

void fat_overwrite_file(const char* filename, const char* text) {
    char fat_name[11];
    to_fat_name(filename, fat_name);
    
    uint8_t cluster_buf[4096]; 
    uint32_t current_cluster = root_cluster;
    int found = 0;
    int entry_index = 0;

    // 1. On cherche l'entrée existante (créée par touch)
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        
        for (int i = 0; i < (sectors_per_cluster * 512) / 32; i++) {
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                entry_index = i;
                found = 1;
                break;
            }
        }
        if (found) break;
        current_cluster = fat_get_next_cluster(current_cluster);
    }

    if (!found) {
        kprintf("Erreur : Fichier non trouve. Utilisez touch d'abord.\n");
        return;
    }

    // 2. On alloue un cluster libre
    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) {
        kprintf("Erreur : Disque plein.\n");
        return;
    }

    // 3. On marque ce cluster comme FIN dans la FAT
    fat_set_cluster_value(new_cluster, 0x0FFFFFFF);

    // 4. On écrit le texte dans le cluster de données
    uint8_t data[4096];
    memset(data, 0, 4096);
    int len = strlen(text);
    memcpy(data, text, len > 4095 ? 4095 : len);
    fat_write_cluster(new_cluster, data);

    // 5. Mise à jour de l'entrée dans le répertoire
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    entries[entry_index].cluster_low = (uint32_t)(new_cluster & 0xFFFF);
    entries[entry_index].cluster_high = (uint32_t)((new_cluster >> 16) & 0xFFFF);
    entries[entry_index].size = len;

    // Sauvegarde du cluster du répertoire
    fat_write_cluster(current_cluster, cluster_buf);
    kprintf("Fichier mis a jour.\n");
}

void fat_create_file(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name); // Convertit "toto.txt" -> "TOTO    TXT"

    // 1. Lire le secteur du répertoire racine (Secteur 132)
    // On utilise un buffer statique pour éviter de saturer la pile (stack)
    static uint8_t root_buf[512] __attribute__((aligned(4096)));
    
    // On choisit le secteur en fonction du cluster actuel
    uint32_t sector = (current_dir_cluster == 0) ? root_dir_sector : cluster_to_lba(current_dir_cluster);
    
    asm volatile("cli");
    ata_read_sector(sector, (uint16_t*)root_buf);
    asm volatile("sti");

    struct fat_directory_entry* entries = (struct fat_directory_entry*)root_buf;
    int empty_slot = -1;

    // Chercher un emplacement vide dans le secteur lu
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            empty_slot = i;
            break;
        }
    }

    if (empty_slot == -1) {
        kprintf("[FS] Erreur : Repertoire plein.\n");
        return;
    }

    if (empty_slot == -1) {
        kprintf("[FS] Erreur : Repertoire racine plein (max 16 fichiers pour le test).\n");
        return;
    }

    // 3. Allouer un cluster pour que le fichier puisse recevoir des données
    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) {
        kprintf("[FS] Erreur : Impossible d'allouer un cluster (disque plein).\n");
        return;
    }

    // 4. Configurer la nouvelle entrée (32 octets)
    // On commence par mettre tout l'emplacement à zéro
    uint8_t* entry_ptr = (uint8_t*)&entries[empty_slot];
    for (int j = 0; j < 32; j++) entry_ptr[j] = 0;

    // Copie du nom formaté FAT
    for (int j = 0; j < 11; j++) {
        entries[empty_slot].name[j] = fat_name[j];
    }

    entries[empty_slot].attributes = 0x20;        // Attribut Archive
    entries[empty_slot].first_cluster_low = new_cluster;
    entries[empty_slot].file_size = 0;            // Initialement vide

    uint32_t ts = get_fat_timestamp();
    uint32_t current_date = (uint32_t)(ts >> 16);
    uint32_t current_time = (uint32_t)(ts & 0xFFFF);

    entries[empty_slot].creation_time = current_time;
    entries[empty_slot].creation_date = current_date;
    entries[empty_slot].last_modification_time = current_time;
    entries[empty_slot].last_modification_date = current_date;

    // 5. Sauvegarder le répertoire racine mis à jour sur le disque
    //kprintf("[FS] Creation de %s (Cluster : %d)...\n", filename, new_cluster);
    
    // On écrit dans le secteur qu'on a calculé plus haut
    asm volatile("cli");
    ata_write_sector(sector, (uint16_t*)root_buf);
    asm volatile("sti");

    kprintf("[FS] Fichier %s cree dans le cluster %d.\n", filename, current_dir_cluster);
}

// // Charge un fichier complet en mémoire à une adresse spécifique 
int fat_read_file(char* name, uint8_t* buffer) {
    int cluster = fat_find_file_cluster(name);
    if (cluster < 2) return 0;

    uint32_t current_cluster = (uint32_t)cluster;
    uint8_t* current_pos = buffer;

    while (current_cluster >= 2 && current_cluster < 0xFFF8) {
        // LBA du début du cluster
        uint32_t lba = data_sector + (current_cluster - 2) * bpb.sectors_per_cluster;
        
        // BOUCLE INTERNE : Lire chaque secteur du cluster
        for (uint8_t i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(lba + i, (uint16_t*)current_pos);
            current_pos += 512;
        }

        current_cluster = fat_get_next_cluster(current_cluster);
        if (current_cluster == 0) break; 
    }
    return 1;
}

/*void fat_remove_file(const char* filename) {
    // Sécurité : interdire la suppression des entrées système
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        kprintf("[FS] Erreur : Impossible de supprimer les liens relatifs.\n");
        return;
    }

    char fat_name[11];
    to_fat_name(filename, fat_name);

    uint32_t sector = (current_dir_cluster == 0) ? root_dir_sector : cluster_to_lba(current_dir_cluster);
    static uint8_t dir_buf[512] __attribute__((aligned(4096)));
    ata_read_sector(sector, (uint16_t*)dir_buf);

    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;
    int found_idx = -1;

    for (int i = 0; i < 16; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        kprintf("[FS] Erreur : '%s' non trouve.\n", filename);
        return;
    }

    uint32_t first_cluster = entries[found_idx].cluster_low;
    uint8_t is_dir = (entries[found_idx].attributes & 0x10);

    // 3. SI C'EST UN DOSSIER : Vérifier s'il est vide (optionnel mais conseillé)
    // Pour simplifier ici, on libère juste le cluster du dossier.
    
    // 4. Marquer l'entrée comme supprimée (0xE5)
    dir_buf[found_idx * 32] = 0xE5; 
    ata_write_sector(sector, (uint16_t*)dir_buf);

    // 5. Libérer TOUTE la chaîne de clusters
    if (first_cluster >= 2) {
        fat_free_cluster_chain(first_cluster);
    }

    kprintf("[FS] %s '%s' supprime (Clusters liberes).\n", is_dir ? "Dossier" : "Fichier", filename);
}*/

/*void fat_remove_file(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // 1. Trouver l'entrée du fichier
    uint32_t sector;
    int found_idx = -1;
    uint8_t dir_buf[512];
    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;

    // On cherche dans le répertoire actuel (simplifié au premier secteur pour l'exemple)
    sector = fat_cluster_to_lba(current_dir_cluster);
    ata_read_sector(sector, (uint16_t*)dir_buf);

    for (int i = 0; i < 16; i++) { // 16 entrées par secteur
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        kprintf("Erreur : Fichier non trouve.\n");
        return;
    }

    // 2. Récupérer le premier cluster pour libérer la FAT
    uint32_t first_cluster = entries[found_idx].cluster_low | (entries[found_idx].cluster_high << 16);

    // 3. Supprimer l'entrée (Marqueur 0xE5)
    entries[found_idx].name[0] = 0xE5; 
    ata_write_sector(sector, (uint16_t*)dir_buf);

    // 4. Libérer la chaîne de clusters dans la FAT
    if (first_cluster >= 2) {
        fat_free_cluster_chain(first_cluster);
    }

    kprintf("Fichier %s supprime.\n", filename);
}*/

void fat_remove_file(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    uint8_t dir_buf[512];
    // On cible le répertoire actuel
    uint32_t sector = fat_cluster_to_lba(current_dir_cluster == 0 ? root_cluster : current_dir_cluster);
    ata_read_sector(sector, (uint16_t*)dir_buf);

    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;

    for (int i = 0; i < 16; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            // 1. Récupérer le premier cluster de la chaîne
            uint32_t first_cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);

            // 2. Marquer l'entrée de répertoire comme libre (0xE5)
            entries[i].name[0] = 0xE5;
            ata_write_sector(sector, (uint16_t*)dir_buf);

            // 3. Libérer toute la chaîne dans la FAT
            if (first_cluster >= 2) {
                uint32_t current = first_cluster;
                while (current < 0x0FFFFFF8 && current >= 2) {
                    uint32_t next = fat_get_next_cluster(current);
                    fat_set_cluster_value(current, 0); // 0 = Cluster libre
                    current = next;
                }
            }
            kprintf("Fichier %s supprime avec succes.\n", filename);
            return;
        }
    }
    kprintf("Erreur : Impossible de trouver le fichier '%s'.\n", filename);
}


/*void fat_copy_file(const char* src_name, const char* dest_name) {
    // 1. Trouver le fichier source et récupérer son cluster
    int src_cluster = fat_find_file_cluster(src_name);
    if (src_cluster < 2) {
        kprintf("[FS] Erreur : Fichier source '%s' non trouve.\n", src_name);
        return;
    }

    // 2. Lire le contenu du fichier source (on limite à 512 octets pour l'instant)
    static uint8_t copy_buffer[512] __attribute__((aligned(4096)));
    
    uint32_t src_lba = data_sector + (src_cluster - 2) * sectors_per_cluster;
    asm volatile("cli");
    ata_read_sector(src_lba, (uint16_t*)copy_buffer);
    asm volatile("sti");

    // 3. Créer le fichier de destination (cela lui alloue un nouveau cluster)
    fat_create_file(dest_name);

    // 4. Trouver le cluster du nouveau fichier créé
    int dest_cluster = fat_find_file_cluster(dest_name);
    if (dest_cluster < 2) {
        kprintf("[FS] Erreur : Impossible de finaliser la copie.\n");
        return;
    }

    // 5. Écrire les données dans le nouveau cluster
    uint32_t dest_lba = data_sector + (dest_cluster - 2) * sectors_per_cluster;
    asm volatile("cli");
    ata_write_sector(dest_lba, (uint16_t*)copy_buffer);
    asm volatile("sti");

    kprintf("[FS] Copie de '%s' vers '%s' reussie.\n", src_name, dest_name);
}*/

void fat_copy_file(const char* src_name, const char* dest_name) {
    // On laisse fat_find_file_cluster gérer la conversion si elle le fait déjà
    uint32_t src_cluster = (uint32_t)fat_find_file_cluster(src_name);
    
    if (src_cluster == 0xFFFFFFFF) {
        kprintf("Erreur : Fichier source introuvable.\n");
        return;
    }

    // Lire le contenu du premier cluster (512 octets)
    uint8_t buffer[512];
    fat_read_cluster(src_cluster, buffer);

    // Créer la destination (fat_touch gère la conversion)
    if ((uint32_t)fat_find_file_cluster(dest_name) == 0xFFFFFFFF) {
        fat_touch(dest_name);
    }

    // Écrire (fat_overwrite gère la conversion)
    fat_overwrite_file_content(dest_name, (const char*)buffer);
}

/*
Pour créer un dossier, nous devons :
    Trouver une entrée libre dans le dossier actuel (ou le root).
    Lui donner un nom et l'attribut 0x10.
    Trouver un cluster vide dans la FAT pour stocker le contenu du nouveau dossier.
    Initialiser ce nouveau cluster avec les entrées . et ..
*/
int fat_mkdir(char* name) {
    // 1. Vérifier si le nom existe déjà dans le répertoire courant
    char fat_name[11];
    to_fat_name(name, fat_name);
    uint8_t dummy[32];
    if (fat_get_entry(fat_name, dummy) == 0) {
        kprintf("Erreur : Le nom '%s' est deja utilise.\n", name);
        return -1;
    }

    // 2. Trouver un cluster libre pour le contenu du nouveau dossier
    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) {
        kprintf("Erreur : Disque plein.\n");
        return -1;
    }

    // 3. Marquer le cluster comme occupé (Fin de chaîne)
    fat_set_cluster_value(new_cluster, 0xFFFF);

    // 4. Créer l'entrée dans le répertoire parent (celui où on se trouve)
    fat_create_entry(name, 0x10, new_cluster);

    // 5. Initialiser le nouveau cluster avec "." et ".."
    fat_init_sub_directory(new_cluster, current_dir_cluster);

    kprintf("Dossier '%s' cree (Cluster %d).\n", name, new_cluster);
    return 0;
}

int is_directory(char* name) {
    char fat_name[11];
    to_fat_name(name, fat_name);

    uint8_t entry[32];
    // Appel à la fonction maintenant définie
    if (fat_get_entry(fat_name, entry) == -1) return 0;

    // L'attribut est au 12ème octet (index 11)
    // Le bit 4 (0x10) indique un sous-répertoire
    return (entry[11] & 0x10) != 0;
}

int fat_get_entry(char* fat_name, uint8_t* buffer_out) {
    uint8_t directory_buffer[512];
    uint32_t start_sector;
    int sectors_to_read;

    if (current_dir_cluster == 0) {
        start_sector = root_dir_sector;
        sectors_to_read = 14; // On scanne tout le Root
    } else {
        start_sector = cluster_to_lba(current_dir_cluster);
        sectors_to_read = 1; // Pour un sous-dossier simple
    }

    for (int s = 0; s < sectors_to_read; s++) {
        ata_read_sector(start_sector + s, (uint16_t*)directory_buffer);

        for (int i = 0; i < 512; i += 32) {
            if (directory_buffer[i] == 0x00) continue; // Chercher plus loin
            if ((uint8_t)directory_buffer[i] == 0xE5) continue;

            if (memcmp(&directory_buffer[i], fat_name, 11) == 0) {
                memcpy(buffer_out, &directory_buffer[i], 32);
                return 0; // Trouvé !
            }
        }
    }
    return -1; // Réellement introuvable
}

int fat_cd(char* path) {
    // Cas particulier : "cd .."
    if (strcmp(path, "..") == 0) {
        uint8_t entry[32];
        char fat_name[11];
        to_fat_name("..", fat_name);

        if (fat_get_entry(fat_name, entry) == 0) {
            // Le cluster du parent est aux octets 26-27
            uint32_t parent_cluster = entry[26] | (entry[27] << 8);
            current_dir_cluster = parent_cluster;
            return 0;
        }
        return -1;
    }

    // Cas standard : chercher le dossier par son nom
    char fat_name[11];
    to_fat_name(path, fat_name);

    uint8_t entry[32];
    if (fat_get_entry(fat_name, entry) == -1) {
        kprintf("Erreur : Dossier '%s' introuvable.\n", path);
        return -1;
    }

    // Vérifier si c'est bien un répertoire (Attribut 0x10)
    if (!(entry[11] & 0x10)) {
        kprintf("Erreur : '%s' n'est pas un dossier.\n", path);
        return -1;
    }

    // Mettre à jour le cluster actuel
    current_dir_cluster = entry[26] | (entry[27] << 8);
    
    #if DEBUG
    kprintf("[FS] Changement de repertoire vers cluster %d\n", current_dir_cluster);
    #endif

    return 0;
}

/*
Pour savoir si les fichiers sont vraiment là, on va afficher le contenu brut du secteur du répertoire racine. 
Si tu vois le nom de tes fichiers manquants dans le dump mais pas dans le ls, le bug est dans ta logique de boucle for.
*/

void fat_debug_root_dump() {
    uint8_t dump_buf[512];
    kprintf("--- DEBUG ROOT DUMP (LBA %d) ---\n", root_dir_sector);
    
    // On lit le premier secteur du Root
    ata_read_sector(root_dir_sector, (uint16_t*)dump_buf);

    for (int i = 0; i < 512; i++) {
        // Affiche le caractère si imprimable, sinon un point
        char c = (dump_buf[i] >= 32 && dump_buf[i] <= 126) ? dump_buf[i] : '.';
        putc(c);
        
        // On passe à la ligne tous les 32 octets (une entrée FAT)
        if ((i + 1) % 32 == 0) kprintf(" | Index %d\n", (i + 1) / 32);
    }
}

void load_file(uint32_t start_cluster, uint8_t* destination) {
    uint32_t current_cluster = start_cluster;
    uint8_t* current_pos = destination;

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        // 1. Lire le cluster actuel
        fat_read_cluster(current_cluster, current_pos);
        
        // 2. Avancer le pointeur de destination
        current_pos += (sectors_per_cluster * 512);
        
        // 3. Demander à la FAT quel est le prochain cluster
        current_cluster = fat_get_next_cluster(current_cluster);
        
        #ifdef DEBUG
            kprintf("[FAT] Suivi chaine: next cluster = %d\n", current_cluster);
        #endif
    }
}

void fat_touch(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // 1. Vérifier si le fichier existe déjà
    if (fat_find_file_cluster(fat_name) != (uint32_t)-1) {
        kprintf("Erreur : Le fichier existe deja.\n");
        return;
    }

    // 2. Lire le cluster du répertoire racine
    uint8_t cluster_buf[4096]; 
    uint32_t current_cluster = root_cluster;
    
    // On va simplifier en ne cherchant que dans le premier cluster du Root
    fat_read_cluster(current_cluster, cluster_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        // 3. Trouver une entrée libre (0x00 ou 0xE5)
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            
            // 4. Remplir l'entrée
            memset(&entries[i], 0, sizeof(struct fat_dir_entry));
            memcpy(entries[i].name, fat_name, 11);
            entries[i].attributes = 0x20; // Archive
            entries[i].cluster_low = 0;   // Pas encore de données
            entries[i].cluster_high = 0;
            entries[i].size = 0;          // Taille vide

            // 5. Écrire le cluster modifié sur le disque
            fat_write_cluster(current_cluster, cluster_buf);
            
            kprintf("Fichier %s cree avec succes.\n", filename);
            return;
        }
    }

    kprintf("Erreur : Repertoire racine plein.\n");
}

void fat_echo(const char* text, const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name); // Conversion indispensable

    uint32_t cluster = fat_find_file_cluster(fat_name);

    if (cluster == (uint32_t)-1) {
        // Si on doit le créer, fat_touch fera sa propre conversion
        fat_touch(filename);
    }
    
    // On appelle l'écriture
    fat_overwrite_file_content(filename, text);
}
