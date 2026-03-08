#include "fat.h"
#include "ata.h"
#include "../kernel/gos_memory.h"
#include "../kernel/terminal.h"
#include "../lib/string.h"
#include "../kernel/time.h"
#include "../kernel/rtc.h"
#include "../shell/shell.h"
#include "cmos.h"
#include "config.h"

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
    static uint8_t cluster_buf[4096]; 
    char fat_name[11];
    
    // On convertit le nom au format FAT si ce n'est pas déjà fait
    if (strcmp(name_to_find, "..") == 0) {
        memcpy(fat_name, "..         ", 11);
    }  else  if (strcmp(name_to_find, ".") == 0) {
        memcpy(fat_name, ".          ", 11);
    }  else {
        to_fat_name(name_to_find, fat_name);
    }

    // Cherche dans le dossier actuel
    uint32_t current_cluster = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) return 0xFFFFFFFF; // Fin de rép
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; // Supprimé
            if (entries[i].attributes == 0x0F) continue;      // LFN
            #if DEBUG
                kprintf("Scan: [%.11s] Attr: 0x%x\n", entries[i].name, entries[i].attributes);
            #endif
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                return ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
            }
        }
        // CRUCIAL : Passer au cluster suivant pour éviter la boucle infinie !
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    return 0xFFFFFFFF;
}

// Cette fonction écrit dans la table FAT elle-même pour dire "ce cluster est occupé".
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

    current_dir_cluster=root_cluster;

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
/*void fat_ls(const char* target_dir){
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    // UTILISER LE DOSSIER ACTUEL :
    // Si current_dir_cluster est 0 (ou root_cluster), on lit la racine
    uint32_t current_cluster = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;

    // if (target_dir == NULL || strlen(target_dir) == 0) {
    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        // ls standard : dossier actuel
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        // ls <dossier> : on cherche le cluster du dossier cible
        cluster_to_list = fat_find_file_cluster(target_dir);
    }
        if (cluster_to_list == 0xFFFFFFFF) {
            kprintf("Dossier introuvable.\n");
            return;
        }
    //}
    #if DEBUG
        kprintf("Listing du repertoire (Cluster %d) :\n", current_cluster);
    #else
        kprintf("Liste");
    #endif
    
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; // Ignorer LFN

            // --- DEBUT CORRECTION NOMS TRONQUÉS ---
            char clean_name[13];
            int p = 0;

            // 1. Copier le nom (8 caractères)
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
            }

            // 2. Ajouter le point SEULEMENT si ce n'est pas un répertoire
            // Et SEULEMENT s'il y a une extension
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    if(entries[i].name[8] != ' ')
                        clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';
            // --- FIN CORRECTION ---

            if (entries[i].attributes & 0x10) {
                kprintf("  [DIR] %s\n", clean_name);
            } else {
                kprintf("        %s  (%d octets)\n", clean_name, entries[i].size);
            }
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }
}*/

void fat_ls(const char* target_dir) {
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    int long_format = 0;

    // Détection du flag -l
    if (target_dir != NULL && strcmp(target_dir, "-l") == 0) {
        long_format = 1;
        target_dir = NULL; // On liste le dossier actuel
    }

    // Détermination du cluster à lister
    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        cluster_to_list = fat_find_file_cluster(target_dir);
    }

    if (cluster_to_list == 0xFFFFFFFF) {
        kprintf("Dossier introuvable.\n");
        return;
    }

    if (long_format) {
        kprintf("Nom             Taille       Date         Heure\n");
        kprintf("---------------------------------------------------\n");
    }

    uint32_t current_cluster = cluster_to_list;

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; 

            // Nettoyage du nom (8.3)
            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
            }
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';

            if (long_format) {
                int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);

                // 1. Afficher le nom
                kprintf("%s", clean_name);
                // On veut que la colonne "Nom" fasse 16 caractères
                print_padding(strlen(clean_name), 16); 

                if (entries[i].attributes & 0x10) {
                    // Cas Dossier
                    kprintf("<DIR>");
                    print_padding(5, 12); // " <DIR> " prend 5 caractères, on aligne sur 12
                } else {
                    // Cas Fichier
                    kprintf("%d bytes", entries[i].size);
                    // On calcule la longueur de "X bytes" (taille + espace + 5 lettres)
                    int len_size = get_int_len(entries[i].size) + 6; 
                    print_padding(len_size, 12);
                }

                // 2. Afficher la Date et l'Heure
                //int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);

                // Pour les dates, on force le 0 devant manuellement si kprintf ne le fait pas
                // 1. LE JOUR
                if (d < 10) 
                    putc('0');
                putd(d);
                putc('/');

                // 2. LE MOIS
                if (mon < 10) 
                    putc('0');
                putd(mon);
                putc('/');

                // 3. L'ANNÉE
                putd(y);
                kprintf("  "); // Espacement entre date et heure

                // 4. L'HEURE
                if (h < 10) 
                    putc('0');
                putd(h);
                putc(':');

                // 5. LES MINUTES
                if (min < 10) 
                    putc('0');
                putd(min);

                kprintf("\n");

                // Format: FICHIER.TXT    128 bytes    01/03/2026 14:30
                // On utilise %-15s pour aligner le nom à gauche sur 15 caractères
                // if (entries[i].attributes & 0x10) {
                //     kprintf("%-15s <DIR>        %02d/%02d/%d    %02d:%02d\n", 
                //             clean_name, d, mon, y, h, min);
                // } else {
                //     kprintf("%-15s %d bytes    %02d/%02d/%d    %02d:%02d\n", 
                //             clean_name, entries[i].size, d, mon, y, h, min);
                // }
            } else {
                // Affichage simple
                if (entries[i].attributes & 0x10) kprintf("[%s] ", clean_name);
                else kprintf("%s  ", clean_name);
            }
        }
        if (!long_format) kprintf("\n");
        current_cluster = fat_get_next_cluster(current_cluster);
    }
}

// Cette fonction va lire la table FAT (qui commence au secteur 4 sur ton disque) et chercher la première case vide.
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

/*void fat_free_cluster_chain(uint32_t cluster) {
    uint32_t current = cluster;
    while (current < 0x0FFFFFF8 && current >= 2) {
        uint32_t next = fat_get_next_cluster(current);
        fat_set_cluster_value(current, 0); // Libère le cluster actuel
        current = next;
    }
}*/

void fat_free_cluster_chain(uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= 0x0FFFFFF8) return;

    uint32_t current = start_cluster;
    while (current < 0x0FFFFFF8 && current >= 2) {
        uint32_t next = fat_get_next_cluster(current);
        
        // On remet l'entrée à 0 dans la FAT pour indiquer que le cluster est libre
        fat_set_cluster_value(current, 0);
        
        current = next;
    }
}


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

/*gère le rembourrage d'espaces 
n FAT, les noms de fichiers sont un peu particuliers : ils doivent toujours faire 11 caractères 
(8 pour le nom, 3 pour l'extension) complétés par des espaces.

Par exemple, "TEST.TXT" doit devenir "TEST TXT".*/
/*void to_fat_name(const char* input, char* output) {
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
}*/
void to_fat_name(const char* input, char* output) {
    memset(output, ' ', 11); // Remplir d'espaces par défaut

    // Cas spécial : Dossier actuel "."
    if (strcmp(input, ".") == 0) {
        output[0] = '.';
        return;
    }

    // Cas spécial : Dossier parent ".."
    if (strcmp(input, "..") == 0) {
        output[0] = '.';
        output[1] = '.';
        return;
    }

    int i = 0, j = 0;

    // 1. Nom principal (8 char max)
    // On s'arrête si on croise un point ou la fin de la chaîne
    while (input[i] != '\0' && input[i] != '.' && j < 8) {
        if (input[i] >= 'a' && input[i] <= 'z') 
            output[j++] = input[i++] - 32; // Mise en majuscule
        else 
            output[j++] = input[i++];
    }

    // 2. Sauter les caractères du nom principal si > 8 (troncation)
    // et chercher l'extension après le point
    const char* ext = 0;
    for (int k = 0; input[k] != '\0'; k++) {
        if (input[k] == '.') ext = &input[k + 1];
    }

    // 3. Si on a trouvé une extension, on la remplit à partir de l'index 8
    if (ext) {
        j = 8;
        int k = 0;
        while (ext[k] != '\0' && k < 3) {
            if (ext[k] >= 'a' && ext[k] <= 'z') 
                output[j++] = ext[k++] - 32;
            else 
                output[j++] = ext[k++];
        }
    }
}

// lit et affiche cluster par cluster
/*void fat_cat(const char* filename) {
    #if DEBUG
        kprintf("[DEBUG] cat cherche %s. Root Cluster: %d, Data LBA: %d\n", filename, root_cluster, data_lba);
    #endif
    char fat_name[11];
    to_fat_name(filename, fat_name); // "notes.txt" -> "NOTES   TXT"
    // 1. Trouver le fichier et récupérer son entrée complète
    // On a besoin de la taille exacte
    static uint8_t cluster_buf[4096]; 
    //uint32_t current_cluster = root_cluster;
    uint32_t current_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
    struct fat_dir_entry file_entry;
    int found = 0;

    // // --- Recherche du fichier ---
    // while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
    //     fat_read_cluster(current_cluster, cluster_buf);
    //     struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        
    //     // Calcule le nombre d'entrées (souvent 128 pour un cluster de 4ko)
    //     int entries_per_cluster = (sectors_per_cluster * 512) / 32;

    //     for (int i = 0; i < entries_per_cluster; i++) {
    //         // Si le premier octet est 0, c'est la fin du répertoire
    //         if (entries[i].name[0] == 0) {
    //             // kprintf("Fin de repertoire atteinte a l'entree %d\n", i);
    //             break; 
    //         }

    //         // On affiche enfin ce qu'on lit
    //         // Attention: name n'est pas une chaine terminée par \0, 
    //         // kprintf %s risque de lire trop loin si tu n'utilises pas une limite.
    //         // Utilise plutot une boucle pour afficher les 11 caracteres.
            
    //         if (memcmp(entries[i].name, fat_name, 11) == 0) {
    //             file_entry = entries[i];
    //             found = 1;
    //             break;
    //         }
    //     }
    //     if (found) break;
    //     current_cluster = fat_get_next_cluster(current_cluster);
    // }
    // if (!found) {
    //     kprintf("Fichier non trouve.\n");
    //     return;
    // }

    struct fat_dir_entry entry;
    uint32_t cluster = fat_resolve_path(path, &entry);

    if (cluster == 0xFFFFFFFF || (entry.attributes & 0x10)) {
        kprintf("Erreur : Fichier introuvable ou est un repertoire.\n");
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
}*/

void fat_cat(const char* path) {
    struct fat_dir_entry entry;
    // On résout le chemin complet pour obtenir le cluster et les infos (taille)
    uint32_t file_cluster = fat_resolve_path(path, &entry);

    if (file_cluster == 0xFFFFFFFF) {
        kprintf("Erreur : Impossible de trouver '%s'\n", path);
        return;
    }
    
    if (entry.attributes & 0x10) {
        kprintf("Erreur : '%s' est un repertoire.\n", path);
        return;
    }

    // --- ICI TA LOGIQUE DE LECTURE ---
    static uint8_t cluster_buf[4096];
    uint32_t size_left = entry.size;

    while (size_left > 0 && file_cluster < 0x0FFFFFF8 && file_cluster >= 2) {
        fat_read_cluster(file_cluster, cluster_buf);
        uint32_t to_read = (sectors_per_cluster * 512);
        if (size_left < to_read) to_read = size_left;

        for (uint32_t i = 0; i < to_read; i++) {
            kprintf("%c", cluster_buf[i]);
        }

        size_left -= to_read;
        if (size_left > 0) file_cluster = fat_get_next_cluster(file_cluster);
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
/*void fat_update_file_size(const char* filename, uint32_t new_size) {
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
}*/

void fat_update_file_size(const char* filename, uint32_t new_size) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    static uint8_t cluster_buf[4096];
    uint32_t dir_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;

    fat_read_cluster(dir_cluster, cluster_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            entries[i].size = new_size; // Mise à jour de la taille
            
            // Optionnel : mettre à jour l'heure de modification ici
            
            fat_write_cluster(dir_cluster, cluster_buf);
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

/*void fat_overwrite_file_content(const char* filename, const char* new_text) {
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
}*/

void fat_overwrite_file_content(const char* filename, const char* new_text) {
    uint32_t first_cluster = fat_find_file_cluster(filename);
    
    // Si le fichier est vide (cluster 0 dans l'entrée), on doit lui donner son premier cluster
    if (first_cluster < 2 || first_cluster >= 0x0FFFFFF8) {
        first_cluster = fat_find_free_cluster();
        if (first_cluster == 0) return;
        fat_set_cluster_value(first_cluster, 0x0FFFFFF8);
        // /!\ IMPORTANT : Il faut aussi mettre à jour l'entrée de répertoire 
        // pour que cluster_low/high pointent vers ce first_cluster.
        fat_update_file_first_cluster(filename, first_cluster);
    }

    int total_len = strlen(new_text);
    int remaining_len = total_len;
    uint32_t current_cluster = first_cluster;
    int text_offset = 0;

    static uint8_t cluster_buffer[4096]; // Taille max d'un cluster

    while (remaining_len > 0) {
        memset(cluster_buffer, 0, 4096);
        
        // On calcule combien on peut mettre dans CE cluster (4096 octets)
        int cluster_size = sectors_per_cluster * 512;
        int to_write = (remaining_len > cluster_size) ? cluster_size : remaining_len;
        
        memcpy(cluster_buffer, new_text + text_offset, to_write);

        // Écrire TOUT le cluster (plusieurs secteurs si nécessaire)
        fat_write_cluster(current_cluster, cluster_buffer);

        remaining_len -= to_write;
        text_offset += to_write;

        if (remaining_len > 0) {
            uint32_t next = fat_get_next_cluster(current_cluster);
            
            if (next >= 0x0FFFFFF8) { 
                next = fat_find_free_cluster();
                if (next == 0) {
                    kprintf("Erreur : Disque plein.\n");
                    break;
                }
                fat_set_cluster_value(current_cluster, next);
                fat_set_cluster_value(next, 0x0FFFFFF8); // Marqueur FAT32
            }
            current_cluster = next;
        }
        else {
            // --- LOGIQUE DE LIBÉRATION DES CLUSTERS EN TROP ---
            
            // On regarde s'il y avait une suite à la chaîne de clusters
            uint32_t next_to_free = fat_get_next_cluster(current_cluster);
            
            // 1. On marque le cluster actuel comme "Fin de fichier" (EOF)
            fat_set_cluster_value(current_cluster, 0x0FFFFFF8);
            
            // 2. Si un cluster suivait, on libère toute la branche désormais inutile
            if (next_to_free < 0x0FFFFFF8 && next_to_free >= 2) {
                fat_free_cluster_chain(next_to_free);
            }
        }
    }

    fat_update_file_size(filename, total_len);
}

void fat_overwrite_file(const char* filename, const char* text) {
    char fat_name[11];
    to_fat_name(filename, fat_name);
    
    static uint8_t cluster_buf[4096]; 
    //uint32_t current_cluster = root_cluster;
    uint32_t current_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
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

/*void fat_create_file(const char* filename) {
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
    for (int j = 0; j < 32; j++) 
        entry_ptr[j] = 0;

    uint32_t ts = get_fat_timestamp();
    uint32_t current_date = (uint32_t)(ts >> 16);
    uint32_t current_time = (uint32_t)(ts & 0xFFFF);

    // Copie du nom formaté FAT
    for (int j = 0; j < 11; j++) {
        entries[empty_slot].name[j] = fat_name[j];
        entries[j].attributes = 0x20; // Archive (Fichier normal)
        entries[j].last_modification_time = current_time;
        entries[j].last_modification_date = current_date;
        entries[j].creation_time = current_time;
        entries[j].creation_date = current_date;
        entries[j].first_cluster_low = new_cluster & 0xFFFF;
        //entries[j].cluster_high = (new_cluster >> 16) & 0xFFFF;
        entries[j].file_size = 0; // Sera mis à jour après l'écriture du texte
    }

    entries[empty_slot].attributes = 0x20;        // Attribut Archive
    entries[empty_slot].first_cluster_low = new_cluster;
    entries[empty_slot].file_size = 0;            // Initialement vide

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
}*/

void fat_create_file(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    static uint8_t root_buf[512] __attribute__((aligned(4096)));
    uint32_t sector = (current_dir_cluster == 0) ? root_dir_sector : cluster_to_lba(current_dir_cluster);
    
    asm volatile("cli");
    ata_read_sector(sector, (uint16_t*)root_buf);
    asm volatile("sti");

    struct fat_directory_entry* entries = (struct fat_directory_entry*)root_buf;
    int empty_slot = -1;

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

    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) return;

    // --- CONFIGURATION DE L'ENTRÉE ---
    // On nettoie l'entrée choisie
    memset(&entries[empty_slot], 0, sizeof(struct fat_directory_entry));

    // On récupère l'heure réelle via le CMOS
    uint16_t current_time = get_fat_time_rtc();
    uint16_t current_date = get_fat_date_rtc();

    // Copie du nom (les 11 caractères)
    memcpy(entries[empty_slot].name, fat_name, 11);

    // Configuration des attributs et dates
    entries[empty_slot].attributes = 0x20; // Archive
    entries[empty_slot].creation_time = current_time;
    entries[empty_slot].creation_date = current_date;
    entries[empty_slot].last_modification_time = current_time;
    entries[empty_slot].last_modification_date = current_date;
    
    // Cluster (Haut et Bas pour FAT32)
    entries[empty_slot].first_cluster_low = (uint16_t)(new_cluster & 0xFFFF);
    entries[empty_slot].first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    
    entries[empty_slot].file_size = 0;

    // --- SAUVEGARDE ---
    asm volatile("cli");
    ata_write_sector(sector, (uint16_t*)root_buf);
    asm volatile("sti");

    // Important : Marquer le cluster comme "occupé" dans la FAT sinon il sera réutilisé !
    fat_set_cluster_value(new_cluster, 0x0FFFFFFF); 

    kprintf("[FS] Fichier %s cree avec succes a %02d:%02d.\n", filename, (current_time >> 11), (current_time >> 5) & 0x3F);
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

void fat_remove_file(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // On utilise un buffer statique pour ne pas saturer la pile
    static uint8_t cluster_buf[4096]; 
    uint32_t dir_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;

    // 1. Lire TOUT le cluster du répertoire parent
    fat_read_cluster(dir_cluster, cluster_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break; // Fin de liste

        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            // SÉCURITÉ : Vérifier que ce n'est pas un répertoire (Attribut 0x10)
            // Si tu veux supprimer un répertoire, il vaut mieux faire une fonction rmdir
            if (entries[i].attributes & 0x10) {
                kprintf("Erreur : '%s' est un repertoire. Utilisez rmdir.\n", filename);
                return;
            }

            // 1. Récupérer le premier cluster
            uint32_t first_cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);

            // 2. Marquer l'entrée comme libre (0xE5)
            entries[i].name[0] = 0xE5;
            fat_write_cluster(dir_cluster, cluster_buf);

            // 3. Libérer la chaîne dans la FAT
            uint32_t current = first_cluster;
            while (current < 0x0FFFFFF8 && current >= 2) {
                uint32_t next = fat_get_next_cluster(current);
                fat_set_cluster_value(current, 0); 
                current = next;
            }

            kprintf("Fichier %s supprime.\n", filename);
            return;
        }
    }
    kprintf("Erreur : Fichier introuvable.\n");
}

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
void fat_mkdir(const char* dirname) {
    char fat_name[11];
    to_fat_name(dirname, fat_name);

    // 1. Allouer un cluster pour le nouveau dossier
    uint32_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0xFFFFFFFF) return;
    fat_set_cluster_value(new_cluster, 0x0FFFFFF8); // Marqueur fin de chaîne

    uint16_t now_time = get_fat_time_rtc();
    uint16_t now_date = get_fat_date_rtc();

    // 2. Créer l'entrée dans le répertoire COURANT (Parent)
    uint8_t dir_buf[4096];
    uint32_t dir_cluster = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    fat_read_cluster(dir_cluster, dir_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;
    
    int found = 0;
    for (int i = 0; i < (sectors_per_cluster * 512) / 32; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            memset(&entries[i], 0, sizeof(struct fat_dir_entry));
            memcpy(entries[i].name, fat_name, 11);
            entries[i].attributes = 0x10; // Attribut Dossier
            entries[i].last_mod_time = now_time;
            entries[i].last_mod_date = now_date;
            entries[i].creation_time = now_time;
            entries[i].creation_date = now_date;
            entries[i].cluster_low = new_cluster & 0xFFFF;
            entries[i].cluster_high = (new_cluster >> 16) & 0xFFFF;
            fat_write_cluster(dir_cluster, dir_buf);
            found = 1;
            break;
        }
    }

    if (!found) { kprintf("Erreur : Repertoire parent plein.\n"); return; }

    // 3. INITIALISER le cluster du nouveau dossier (Création de . et ..)
    uint8_t new_dir_content[4096]; 
    memset(new_dir_content, 0, 4096); // On nettoie tout le cluster
    struct fat_dir_entry* sub = (struct fat_dir_entry*)new_dir_content;

    // Entrée "." (Lui-même)
    memcpy(sub[0].name, ".          ", 11);
    sub[0].attributes = 0x10;
    sub[0].cluster_low = new_cluster & 0xFFFF;
    sub[0].cluster_high = (new_cluster >> 16) & 0xFFFF;
    sub[0].last_mod_time = now_time;
    sub[0].last_mod_date = now_date;

    // Entrée ".." (Le parent)
    memcpy(sub[1].name, "..         ", 11);
    sub[1].attributes = 0x10;
    // En FAT32, si le parent est la racine, on met souvent 0
    uint32_t parent = (current_dir_cluster == root_cluster) ? 0 : current_dir_cluster;
    sub[1].cluster_low = parent & 0xFFFF;
    sub[1].cluster_high = (parent >> 16) & 0xFFFF;
    sub[1].last_mod_time = now_time;
    sub[1].last_mod_date = now_date;

    // On écrit ces 2 entrées dans le cluster fraîchement alloué
    fat_write_cluster(new_cluster, new_dir_content);
    
    //kprintf("Repertoire '%s' initialise avec . et ..\n", dirname);
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
    uint32_t cluster;
    struct fat_dir_entry entry;

    // 1. GESTION DE LA RACINE
    if (strcmp(path, "/") == 0) {
        current_dir_cluster = root_cluster;
        strcpy(current_path_name, "/");
        return 0; // On quitte la fonction ici
    }

    // 2. GESTION DU RETOUR (CD ..)
    if (strcmp(path, "..") == 0) {
        if (current_dir_cluster == root_cluster) {
            // Déjà au sommet, on ne fait rien
            return 0; 
        }
        uint32_t parent_cluster = fat_find_file_cluster("..");
        #if DEBUG
            kprintf("[DEBUG] Cluster parent trouve: %d\n", parent_cluster);
        #endif

        if (parent_cluster != 0xFFFFFFFF) {
            // En FAT32, remonter à la racine renvoie souvent 0
            current_dir_cluster = (parent_cluster == 0) ? root_cluster : parent_cluster;

            if (current_dir_cluster == root_cluster) {
                strcpy(current_path_name, "/");
            } else {
                char* last_slash = strrchr(current_path_name, '/');
                if (last_slash != NULL) {
                    if (last_slash == current_path_name) {
                        current_path_name[1] = '\0';
                    } else {
                        *last_slash = '\0';
                    }
                }
            }
            return 0; // On quitte la fonction ici !
        } else {
            kprintf("Impossible de remonter.\n");
            return -1;
        }
    }

    // 3. GESTION DOSSIER NORMAL
    //cluster = fat_find_file_cluster(path);
    cluster = fat_resolve_path(path, &entry);

    if (cluster != 0xFFFFFFFF) {
        // --- LA VERIFICATION CRITIQUE ---
        // On vérifie si le bit 4 (0x10) de l'attribut est à 1
        if (!(entry.attributes & 0x10)) {
            kprintf("Erreur : '%s' n'est pas un repertoire.\n", path);
            return -1;
        }
        // --------------------------------

        current_dir_cluster = cluster;
        
        // Sécurité de taille
        if (strlen(current_path_name) + strlen(path) + 2 < 256) {
            // Si on n'est pas à la racine, on ajoute un slash de séparation
            if (strcmp(current_path_name, "/") != 0) {
                strcat(current_path_name, "/");
            }
            // On ajoute le nom du dossier
            strcat(current_path_name, path);
        } else {
            kprintf("Erreur : Chemin trop long.\n");
        }
        
        return 0; // On quitte la fonction ici
    }

    kprintf("Repertoire introuvable : %s\n", path);
    return -1;
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
        
        #if DEBUG
            kprintf("[FAT] Suivi chaine: next cluster = %d\n", current_cluster);
        #endif
    }
}

/*void fat_touch(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // 1. Vérifier si le fichier existe déjà
    if (fat_find_file_cluster(fat_name) != (uint32_t)-1) {
        kprintf("Erreur : Le fichier existe deja.\n");
        return;
    }

    // 2. Lire le cluster du répertoire racine
    static uint8_t cluster_buf[4096]; 
    //uint32_t current_cluster = root_cluster;
    uint32_t current_cluster = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;

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

    fat_update_file_timestamp(filename);    

    kprintf("Erreur : Repertoire racine plein.\n");
}*/

void fat_touch(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    if (fat_find_file_cluster(filename) != 0xFFFFFFFF) {
        kprintf("Erreur : Le fichier existe deja.\n");
        return;
    }

    static uint8_t cluster_buf[4096]; 
    uint32_t current_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;

    fat_read_cluster(current_cluster, cluster_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            
            memset(&entries[i], 0, sizeof(struct fat_dir_entry));
            memcpy(entries[i].name, fat_name, 11);
            entries[i].attributes = 0x20; // Archive
            
            // --- AJOUT DE LA DATE ET HEURE ---
            int d, m, y, h, min, s;
            get_current_datetime(&d, &m, &y, &h, &min, &s);
            
            uint16_t fat_time = fat_encode_time(h, min, s);
            uint16_t fat_date = fat_encode_date(d, m, y);

            entries[i].last_mod_time = fat_time;
            entries[i].last_mod_date = fat_date;
            entries[i].creation_time = fat_time; // Optionnel : date de création
            entries[i].creation_date = fat_date;
            // ----------------------------------

            entries[i].cluster_low = 0;
            entries[i].cluster_high = 0;
            entries[i].size = 0;

            fat_write_cluster(current_cluster, cluster_buf);
            
            kprintf("Fichier %s cree avec succes.\n", filename);
            return;
        }
    }

    kprintf("Erreur : Repertoire plein.\n");
}

/*void fat_echo(const char* text, const char* filename) {
    // 1. On cherche d'abord si le fichier existe
    // Note: fat_find_file_cluster DOIT faire le to_fat_name en interne
    uint32_t cluster = fat_find_file_cluster(filename);

    if (cluster == 0xFFFFFFFF) {
        // 2. S'il n'existe pas, on le crée (touch utilise le dossier actuel)
        fat_touch(filename);
    }
    
    // 3. On écrit (assure-toi que cette fonction cherche aussi dans le dossier actuel !)
    fat_overwrite_file_content(filename, text);
}*/

void fat_echo(const char* text, const char* filename) {
    uint32_t cluster = fat_find_file_cluster(filename);

    if (cluster == 0xFFFFFFFF) {
        fat_touch(filename);
    }
    
    // 1. Écrire les données dans les clusters
    fat_overwrite_file_content(filename, text);

    // 2. Mettre à jour la taille réelle dans le répertoire
    uint32_t len = strlen(text);
    fat_update_file_size(filename, len);

    // 3 Mettre à jour la date et l'heure
    fat_update_file_timestamp(filename);
}

int is_dir_empty(uint32_t cluster) {
    static uint8_t buf[4096];
    fat_read_cluster(cluster, buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        // 0x00 = fin de liste, on n'a rien trouvé d'autre
        if (entries[i].name[0] == 0x00) return 1; 
        // 0xE5 = entrée supprimée, on ignore
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;
        // Ignorer les entrées "." et ".."
        if (entries[i].name[0] == '.') {
            if (entries[i].name[1] == ' ' || (entries[i].name[1] == '.' && entries[i].name[2] == ' '))
                continue;
        }
        // Si on arrive ici, on a trouvé un vrai fichier ou dossier
        return 0; 
    }
    return 1;
}

void fat_rmdir(const char* dirname) {
    char fat_name[11];
    to_fat_name(dirname, fat_name);

    static uint8_t cluster_buf[4096];
    uint32_t parent_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;

    fat_read_cluster(parent_cluster, cluster_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;

        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            // 1. Vérifier que c'est bien un répertoire
            if (!(entries[i].attributes & 0x10)) {
                kprintf("Erreur : '%s' n'est pas un repertoire.\n", dirname);
                return;
            }

            uint32_t dir_cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);

            // 2. Vérifier si le répertoire est vide
            if (!is_dir_empty(dir_cluster)) {
                kprintf("Erreur : Le repertoire n'est pas vide.\n");
                return;
            }

            // 3. Supprimer l'entrée dans le parent (0xE5)
            entries[i].name[0] = 0xE5;
            fat_write_cluster(parent_cluster, cluster_buf);

            // 4. Libérer le cluster du répertoire dans la FAT
            fat_set_cluster_value(dir_cluster, 0);

            kprintf("Repertoire '%s' supprime.\n", dirname);
            return;
        }
    }
    kprintf("Erreur : Repertoire introuvable.\n");
}

void fat_update_file_first_cluster(const char* filename, uint32_t cluster) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    static uint8_t buf[4096];
    uint32_t dir_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
    fat_read_cluster(dir_cluster, buf);
    
    struct fat_dir_entry* entries = (struct fat_dir_entry*)buf;
    for (int i = 0; i < (sectors_per_cluster * 512) / 32; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            entries[i].cluster_low = cluster & 0xFFFF;
            entries[i].cluster_high = (cluster >> 16) & 0xFFFF;
            fat_write_cluster(dir_cluster, buf);
            return;
        }
    }
}

// Time: 5 bits heure, 6 bits minute, 5 bits (secondes/2)
uint16_t fat_encode_time(int hour, int min, int sec) {
    return ((hour & 0x1F) << 11) | 
           ((min & 0x3F) << 5)  | 
           ((sec / 2) & 0x1F);
}

// Date: 7 bits année (depuis 1980), 4 bits mois, 5 bits jour
uint16_t fat_encode_date(int day, int month, int year) {
    return (((year - 1980) & 0x7F) << 9) | 
           ((month & 0x0F) << 5)        | 
           (day & 0x1F);
}

void fat_decode_time(uint16_t time, int *h, int *m, int *s) {
    *h = (time >> 11) & 0x1F;
    *m = (time >> 5) & 0x3F;
    *s = (time & 0x1F) * 2; // Rappel : les secondes sont divisées par 2 en FAT
}

void fat_decode_date(uint16_t date, int *d, int *m, int *y) {
    *d = date & 0x1F;
    *m = (date >> 5) & 0x0F;
    *y = ((date >> 9) & 0x7F) + 1980;

    // Petite sécurité pour l'affichage
    if (*d == 0) *d = 1;
    if (*m == 0) *m = 1;
}

void fat_update_file_timestamp(const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name);

    // Récupérer la date/heure actuelle (via ton RTC)
    int d, m, y, h, min, s;
    get_current_datetime(&d, &m, &y, &h, &min, &s);

    static uint8_t buf[4096];
    uint32_t dir_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
    
    fat_read_cluster(dir_cluster, buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)buf;
    int max_entries = (sectors_per_cluster * 512) / 32;

    for (int i = 0; i < max_entries; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            // Mise à jour des champs (les noms dépendent de ta structure fat_dir_entry)
            entries[i].last_mod_time = fat_encode_time(h, min, s);
            entries[i].last_mod_date = fat_encode_date(d, m, y);
            
            fat_write_cluster(dir_cluster, buf);
            return;
        }
    }
}

// Affiche n espaces pour aligner la colonne suivante
void print_padding(int current_len, int target_len) {
    int spaces = target_len - current_len;
    for (int i = 0; i < spaces; i++) {
        putc(' ');
    }
}

uint32_t fat_get_cluster_from_path(const char* path) {
    uint32_t current_cluster;
    char path_copy[256];
    strncpy(path_copy, path, 256);

    // 1. Point de départ
    if (path[0] == '/') {
        current_cluster = root_cluster;
    } else {
        current_cluster = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    }

    // 2. Découpage du chemin (on utilise strtok ou une version maison)
    char* token = strtok(path_copy, "/");
    uint32_t next_cluster = current_cluster;

    while (token != NULL) {
        // On cherche "token" dans le cluster actuel
        next_cluster = fat_find_file_cluster_in_dir(current_cluster, token);
        
        if (next_cluster == 0xFFFFFFFF) {
            return 0xFFFFFFFF; // Chemin invalide
        }

        current_cluster = next_cluster;
        token = strtok(NULL, "/");
    }

    return current_cluster;
}

// Cherche un nom de fichier dans un dossier spécifique (dir_cluster)
// Retourne le premier cluster de l'élément trouvé, ou 0xFFFFFFFF
uint32_t fat_find_in_dir(uint32_t dir_cluster, const char* fat_name, struct fat_dir_entry* out_entry) {
    static uint8_t buf[4096];
    uint32_t current = (dir_cluster < 2) ? root_cluster : dir_cluster;

    while (current < 0x0FFFFFF8 && current >= 2) {
        fat_read_cluster(current, buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)buf;
        int max = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max; i++) {
            if (entries[i].name[0] == 0x00) return 0xFFFFFFFF; // Fin de liste
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; // Fichier supprimé

            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                // On remplit la structure si l'utilisateur l'a demandée
                if (out_entry) *out_entry = entries[i];
                
                uint32_t target = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                
                // En FAT32, le cluster 0 dans une entrée ".." signifie "Racine"
                return (target == 0) ? root_cluster : target;
            }
        }
        current = fat_get_next_cluster(current);
    }
    return 0xFFFFFFFF;
}

/*uint32_t fat_resolve_path(const char* path, struct fat_dir_entry* out_entry) {
    uint32_t current_cluster;
    int i = 0;

    // Déterminer le point de départ
    if (path[0] == '/') {
        current_cluster = root_cluster;
        i = 1; // On saute le premier '/'
    } else {
        current_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
    }

    if (path[i] == '\0') return current_cluster; // Cas "ls /" ou "cd /"

    char component[13];
    while (path[i] != '\0') {
        int c = 0;
        // Extraire le prochain morceau du chemin (ex: "COCORICO")
        while (path[i] != '/' && path[i] != '\0' && c < 12) {
            component[c++] = path[i++];
        }
        component[c] = '\0';
        if (path[i] == '/') i++;

        char fat_name[11];
        to_fat_name(component, fat_name);

        // On cherche ce morceau dans le cluster actuel
        current_cluster = fat_find_in_dir(current_cluster, fat_name, out_entry);

        if (current_cluster == 0xFFFFFFFF) return 0xFFFFFFFF; // Chemin cassé
    }

    return current_cluster;
}*/
uint32_t fat_resolve_path(const char* path, struct fat_dir_entry* out_entry) {
    uint32_t current_cluster;
    int i = 0;

    // 1. Déterminer le point de départ
    if (path[0] == '/') {
        current_cluster = root_cluster;
        i = 1; // On saute le premier '/'
    } else {
        // Chemin relatif : on part d'où on est
        current_cluster = (current_dir_cluster < 2) ? root_cluster : current_dir_cluster;
    }

    // Cas spécial : si on demande juste "/" ou "."
    if (path[i] == '\0' || (path[i] == '.' && path[i+1] == '\0')) {
        return current_cluster;
    }

    char component[13]; // Pour stocker "DOSSIER" ou "FILE.TXT"
    
    while (path[i] != '\0') {
        int c = 0;
        
        // On extrait le nom jusqu'au prochain '/' ou la fin
        while (path[i] != '/' && path[i] != '\0' && c < 12) {
            component[c++] = path[i++];
        }
        component[c] = '\0';

        // Si on a un double slash (ex: //), on saute
        if (c == 0) {
            if (path[i] == '/') i++;
            continue;
        }

        // Conversion en format FAT (ex: "test" -> "TEST       ")
        char fat_name[11];
        to_fat_name(component, fat_name);

        // Recherche du cluster de ce composant dans le dossier actuel
        // On passe out_entry pour récupérer les métadonnées (taille, etc.)
        current_cluster = fat_find_in_dir(current_cluster, fat_name, out_entry);

        if (current_cluster == 0xFFFFFFFF) {
            return 0xFFFFFFFF; // Chemin cassé en cours de route
        }

        // Si on trouve un '/' après le nom, on continue la boucle
        if (path[i] == '/') i++;
    }

    return current_cluster;
}

uint32_t fat_find_file_cluster_in_dir(uint32_t dir_cluster, const char* filename) {
    char fat_name[11];
    to_fat_name(filename, fat_name); // On convertit "mon_dir" en "MON_DIR    "

    static uint8_t cluster_buf[4096];
    // Si on passe 0 ou 1, on commence à la racine par sécurité
    uint32_t current_cluster = (dir_cluster < 2) ? root_cluster : dir_cluster;

    // On parcourt la chaîne de clusters du répertoire (au cas où il contient beaucoup de fichiers)
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        
        int entries_per_cluster = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < entries_per_cluster; i++) {
            // 0x00 = Fin du répertoire
            if (entries[i].name[0] == 0x00) return 0xFFFFFFFF;
            
            // 0xE5 = Entrée supprimée, on l'ignore
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;

            // On compare le nom formaté FAT
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                // On combine les 16 bits hauts et bas pour avoir le cluster 32 bits
                uint32_t found_cluster = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                
                // Note : En FAT32, si found_cluster est 0, cela pointe vers la racine
                if (found_cluster == 0) return root_cluster;
                
                return found_cluster;
            }
        }
        
        // Si pas trouvé dans ce cluster, on passe au suivant dans la FAT
        current_cluster = fat_get_next_cluster(current_cluster);
    }

    return 0xFFFFFFFF;
}

uint16_t get_fat_time() {
    int h = 14, m = 30, s = 0; // Valeurs par défaut ou récupérées du hardware
    return (uint16_t)((h << 11) | (m << 5) | (s / 2));
}

uint16_t get_fat_date() {
    int d = 1, mon = 3, y = 2026; // On est le 1er Mars 2026 !
    return (uint16_t)(((y - 1980) << 9) | (mon << 5) | d);
}


/*int fat_list_to_buffer(const char* target_dir, char* out_buffer) {
    out_buffer[0] = '\0'; // On commence avec une chaîne vide
    
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    int long_format = 0;

    // Détection du flag -l
    if (target_dir != NULL && strcmp(target_dir, "-l") == 0) {
        long_format = 1;
        target_dir = NULL; // On liste le dossier actuel
    }

    // Détermination du cluster à lister
    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        cluster_to_list = fat_find_file_cluster(target_dir);
    }

    if (cluster_to_list == 0xFFFFFFFF) {
        strcat(out_buffer,"Dossier introuvable.\n");
        return -1;
    }

    if (long_format) {
        strcat(out_buffer,"Nom             Taille       Date         Heure\n");
        strcat(out_buffer,"---------------------------------------------------\n");
    }

    uint32_t current_cluster = cluster_to_list;

    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return -1;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; 

            // Nettoyage du nom (8.3)
            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') 
                    clean_name[p++] = entries[i].name[j];
            }
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') 
                            clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';

            if (long_format) {
                int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);

                // 1. Afficher le nom
                strcat(out_buffer, clean_name);

                // On veut que la colonne "Nom" fasse 16 caractères
                char* buffertmp=kmalloc(strlen(clean_name));
                buffertmp[0] = '\0';
                strcat(out_buffer, str_padding(buffertmp,strlen(clean_name), 16));
                kfree(buffertmp);

                if (entries[i].attributes & 0x10) {
                    // Cas Dossier
                    strcat(out_buffer, "<DIR>");
                    char* buffertmp=kmalloc(12);
                    buffertmp[0] = '\0';
                    strcat(out_buffer, str_padding(buffertmp,5,12));
                    kfree(buffertmp);
                } else {
                    // Cas Fichier
                    strcat(out_buffer, (char *) entries[i].size);
                    strcat(out_buffer," bytes");
                    // On calcule la longueur de "X bytes" (taille + espace + 5 lettres)
                    int len_size = get_int_len(entries[i].size) + 6; 
                    char* buffertmp=kmalloc(12);
                    buffertmp[0] = '\0';
                    strcat(out_buffer, str_padding(buffertmp,len_size,12));
                    kfree(buffertmp);
                }

                // 2. Afficher la Date et l'Heure
                //int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);

                // Pour les dates, on force le 0 devant manuellement si kprintf ne le fait pas
                // 1. LE JOUR
                if (d < 10) 
                    strcat(out_buffer, "0");
                strcat(out_buffer, (char *)d);
                strcat(out_buffer, "/");

                // 2. LE MOIS
                if (mon < 10) 
                    strcat(out_buffer, "0");
                strcat(out_buffer, (char *)mon); 
                strcat(out_buffer, "/");

                // 3. L'ANNÉE
                strcat(out_buffer, (char *)y);
                strcat(out_buffer,"  "); // Espacement entre date et heure

                // 4. L'HEURE
                if (h < 10) 
                    strcat(out_buffer, "0");
                strcat(out_buffer, (char *)h);
                strcat(out_buffer, ":");

                // 5. LES MINUTES
                if (min < 10) 
                    strcat(out_buffer, "0");
                strcat(out_buffer, (char *)min);
                strcat(out_buffer, "\n");
            } else {
                // Affichage simple
                if (entries[i].attributes & 0x10) {
                    strcat(out_buffer, "[");
                    strcat(out_buffer, clean_name);
                    strcat(out_buffer, "] ");
                }
                else {
                    kprintf("%s  ", clean_name);
                    strcat(out_buffer, clean_name);
                    strcat(out_buffer, " "); 
                }
            }
        }
        if (!long_format) {
            strcat(out_buffer, "\n");
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    
    return strlen(out_buffer); // On retourne la longueur pour le syscall
}*/

int fat_get_dir_list(const char* target_dir, char* out_buffer) {
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    out_buffer[0] = '\0'; // Initialisation

    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        cluster_to_list = fat_find_file_cluster(target_dir);
    }

    if (cluster_to_list == 0xFFFFFFFF) return -1;

    uint32_t current_cluster = cluster_to_list;
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; 

            // Ton code de nettoyage du nom (identique)
            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
            }
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';

            // Au lieu de kprintf, on ajoute au buffer
            if (entries[i].attributes & 0x10) strcat(out_buffer, "[");
            strcat(out_buffer, clean_name);
            if (entries[i].attributes & 0x10) strcat(out_buffer, "]");
            strcat(out_buffer, " "); // Un simple espace comme séparateur
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    return 0; // Succès
}

int fat_list_to_buffer(const char* target_dir, char* out_buffer) {
    out_buffer[0] = '\0';
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    char tmp_num[32]; // Buffer temporaire pour itoa
    int long_format = 0;
    char line[128]; // Un buffer temporaire pour CHAQUE fichier
    int file_count = 0;
    uint32_t total_bytes = 0;

    struct file_info items[FAT_NB_MAX]; // Limite à 256 fichiers pour ne pas exploser la pile
    //int count = 0;

    // Détection du flag -l
    if (target_dir != NULL && strcmp(target_dir, "-l") == 0) {
        long_format = 1;
        target_dir = NULL;
    }

    // Détermination du cluster (Logique inchangée)
    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        cluster_to_list = fat_find_file_cluster(target_dir);
    }

    if (cluster_to_list == 0xFFFFFFFF) {
        strcat(out_buffer, "Dossier introuvable.\n");
        return -1;
    }

    if (long_format) {
        strcat(out_buffer,"test\n");
        strcat(out_buffer, "Nom             Taille       Date         Heure\n");
        strcat(out_buffer, "---------------------------------------------------\n");
    }

    uint32_t current_cluster = cluster_to_list;
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) break; // Utilise break ici, pas return
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; 

            file_count++;
            if (!(entries[i].attributes & 0x10)) {
                total_bytes += entries[i].size;
            }

            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') 
                    clean_name[p++] = entries[i].name[j];
            }

            // --- PHASE 1 : LECTURE ET STOCKAGE ---
            if (file_count < FAT_NB_MAX) {
                // On copie le nom brut (8+3) pour le tri stable
                strncpy(items[file_count].name, entries[i].name, 11);
                items[file_count].name[11] = '\0'; 
                
                items[file_count].size = entries[i].size;
                items[file_count].date = entries[i].last_mod_date;
                items[file_count].time = entries[i].last_mod_time;
                items[file_count].attr = entries[i].attributes;
                
                file_count++; // On incrémente APRES
            }
        }
        /*
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') 
                    clean_name[p++] = entries[i].name[j];
            }
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') 
                            clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';
            
            // Logique d'ajout au buffer
            if (long_format) {
                int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);
                
                line[0] = '\0'; // On vide la ligne pour ce fichier

                // 1. On ajoute le nom
                strcat(line, clean_name);
                // On aligne le nom (colonne de 16)
                str_padding(line, strlen(line), 16);

                if (entries[i].attributes & 0x10) {
                    strcat(line, "<DIR>");
                } else {
                    itoa(tmp_num, entries[i].size);
                    strcat(line, tmp_num);
                    strcat(line, " bytes");
                }

                // On aligne la fin de la colonne "Taille" à la position 32 (16 pour le nom + 16 pour la taille)
                str_padding(line, strlen(line), 32);

                // Date : JJ/MM/AAAA
                if (d < 10) 
                    strcat(line, "0");
                itoa(tmp_num, d);
                strcat(line, tmp_num); strcat(line, "/");
                if (mon < 10) 
                    strcat(line, "0");
                itoa(tmp_num, mon);
                strcat(line, tmp_num); strcat(line, "/");
                itoa(tmp_num, y);
                strcat(line, tmp_num); strcat(line, "  ");
                 str_padding(line, strlen(line), 45); // On cale l'heure à 45

                // Heure : HH:MM
                if (h < 10) 
                    strcat(line, "0");
                itoa(tmp_num, h);
                strcat(line, tmp_num); strcat(line, ":");
                if (min < 10) 
                    strcat(line, "0");
                itoa(tmp_num, min);
                strcat(line, tmp_num);
                
                strcat(line, "\n");
                strcat(out_buffer, line);
            } else {
                if (entries[i].attributes & 0x10) {
                    strcat(out_buffer, "["); strcat(out_buffer, clean_name); strcat(out_buffer, "] ");
                } else {
                    strcat(out_buffer, clean_name); strcat(out_buffer, "  ");
                }
            }
        }*/
        current_cluster = fat_get_next_cluster(current_cluster);
    }

    // --- PHASE 2 : TRI PAR NOM ---
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = 0; j < file_count - i - 1; j++) {
            // Comparaison alphabétique
            if (strcmp(items[j].name, items[j+1].name) > 0) {
                struct file_info temp = items[j];
                items[j] = items[j+1];
                items[j+1] = temp;
            }
        }
    }

// --- PHASE 3 : AFFICHAGE ---
    for (int i = 0; i < file_count; i++) {
        char line[128];
        line[0] = '\0';
        char clean_name[13];
        int p = 0;

        strcat(line, items[i].name);
        str_padding(line, strlen(line), 16);
        
        for(int j=0; j<8; j++) {
            if(items[i].name[j] != ' ') 
                clean_name[p++] = items[i].name[j];
        }

        if(!(items[i].attr & 0x10)) {
            if(items[i].name[8] != ' ') {
                clean_name[p++] = '.';
                for(int j=8; j<11; j++) {
                    if(items[i].name[j] != ' ') 
                        clean_name[p++] = items[i].name[j];
                }
            }
        }
        clean_name[p] = '\0';
        
        // Logique d'ajout au buffer
        if (long_format) {
            int d, mon, y, h, min, s;
            fat_decode_date(items[i].date, &d, &mon, &y);
            fat_decode_time(items[i].time, &h, &min, &s);
            
            line[0] = '\0'; // On vide la ligne pour ce fichier

            // 1. On ajoute le nom
            strcat(line, clean_name);
            // On aligne le nom (colonne de 16)
            str_padding(line, strlen(line), 16);

            if (items[i].attr & 0x10) {
                strcat(line, "<DIR>");
            } else {
                itoa(tmp_num, items[i].size);
                strcat(line, tmp_num);
                strcat(line, " bytes");
            }

            // On aligne la fin de la colonne "Taille" à la position 32 (16 pour le nom + 16 pour la taille)
            str_padding(line, strlen(line), 32);

            // Date : JJ/MM/AAAA
            if (d < 10) 
                strcat(line, "0");
            itoa(tmp_num, d);
            strcat(line, tmp_num); strcat(line, "/");
            if (mon < 10) 
                strcat(line, "0");
            itoa(tmp_num, mon);
            strcat(line, tmp_num); strcat(line, "/");
            itoa(tmp_num, y);
            strcat(line, tmp_num); strcat(line, "  ");
                str_padding(line, strlen(line), 45); // On cale l'heure à 45

            // Heure : HH:MM
            if (h < 10) 
                strcat(line, "0");
            itoa(tmp_num, h);
            strcat(line, tmp_num); strcat(line, ":");
            if (min < 10) 
                strcat(line, "0");
            itoa(tmp_num, min);
            strcat(line, tmp_num);
            
            strcat(line, "\n");
            //strcat(out_buffer, line);
        } else {
            if (items[i].attr & 0x10) {
                strcat(out_buffer, "["); strcat(out_buffer, clean_name); strcat(out_buffer, "] ");
            } else {
                strcat(out_buffer, clean_name); strcat(out_buffer, "  ");
            }
        }
        
        strcat(out_buffer, line);
    }

    if (long_format) {
        char summary[128];
        summary[0] = '\0';
        strcat(summary, "\n---------------------------------------------------\n");
        
        itoa(tmp_num, file_count);
        strcat(summary, tmp_num);
        strcat(summary, " fichier(s), ");
        
        itoa(tmp_num, total_bytes);
        strcat(summary, tmp_num);
        strcat(summary, " octets au total.\n");
        
        strcat(out_buffer, summary);
    }

    #if DEBUG
        kprintf("DEBUG: Buffer final taille = %d\n", strlen(out_buffer));
    #endif

    return strlen(out_buffer);
}

/*
int fat_list_to_buffer(const char* target_dir, char* out_buffer) {
    out_buffer[0] = '\0';
    uint32_t cluster_to_list;
    static uint8_t cluster_buf[4096];
    char tmp_num[32]; // Buffer temporaire pour itoa
    int long_format = 0;
    char line[128]; // Un buffer temporaire pour CHAQUE fichier
    int file_count = 0;
    uint32_t total_bytes = 0;

    struct file_info list[256]; // Limite à 256 fichiers pour ne pas exploser la pile
    int count = 0;

    // Détection du flag -l
    if (target_dir != NULL && strcmp(target_dir, "-l") == 0) {
        long_format = 1;
        target_dir = NULL;
    }

    // Détermination du cluster (Logique inchangée)
    if (target_dir == NULL || strlen(target_dir) == 0 || strcmp(target_dir, ".") == 0) {
        cluster_to_list = (current_dir_cluster == 0) ? root_cluster : current_dir_cluster;
    } else {
        cluster_to_list = fat_find_file_cluster(target_dir);
    }

    if (cluster_to_list == 0xFFFFFFFF) {
        strcat(out_buffer, "Dossier introuvable.\n");
        return -1;
    }

    if (long_format) {
        strcat(out_buffer,"test\n");
        strcat(out_buffer, "Nom             Taille       Date         Heure\n");
        strcat(out_buffer, "---------------------------------------------------\n");
    }

    uint32_t current_cluster = cluster_to_list;
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        fat_read_cluster(current_cluster, cluster_buf);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)cluster_buf;
        int max_entries = (sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) break; // Utilise break ici, pas return
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue; 

            file_count++;
            if (!(entries[i].attributes & 0x10)) {
                total_bytes += entries[i].size;
            }

            char clean_name[13];
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') 
                    clean_name[p++] = entries[i].name[j];
            }
            if(!(entries[i].attributes & 0x10)) {
                if(entries[i].name[8] != ' ') {
                    clean_name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') 
                            clean_name[p++] = entries[i].name[j];
                    }
                }
            }
            clean_name[p] = '\0';
            
            // Logique d'ajout au buffer
            if (long_format) {
                int d, mon, y, h, min, s;
                fat_decode_date(entries[i].last_mod_date, &d, &mon, &y);
                fat_decode_time(entries[i].last_mod_time, &h, &min, &s);
                
                line[0] = '\0'; // On vide la ligne pour ce fichier

                // 1. On ajoute le nom
                strcat(line, clean_name);
                // On aligne le nom (colonne de 16)
                str_padding(line, strlen(line), 16);

                //strcat(out_buffer, clean_name);
                
                // Remplacement de ton kmalloc par un padding manuel (plus sûr dans le noyau)
                // int pad = 16 - strlen(clean_name);
                // while(pad-- > 0) 
                //     strcat(out_buffer, " ");

                if (entries[i].attributes & 0x10) {
                    // strcat(out_buffer, "<DIR>       ");
                    strcat(line, "<DIR>");
                } else {
                    // itoa(tmp_num, entries[i].size);
                    // strcat(out_buffer, tmp_num);        // Longueur variable !
                    // strcat(out_buffer, " bytes  ");     // Longueur fixe de 8
                    itoa(tmp_num, entries[i].size);
                    strcat(line, tmp_num);
                    strcat(line, " bytes");
                }

                // On aligne la fin de la colonne "Taille" à la position 32 (16 pour le nom + 16 pour la taille)
                // str_padding(out_buffer, strlen(out_buffer), 32);
                // On aligne la fin de la colonne taille à 32 caractères
                str_padding(line, strlen(line), 32);

                // Date : JJ/MM/AAAA
                if (d < 10) 
                    strcat(line, "0");
                itoa(tmp_num, d);
                strcat(line, tmp_num); strcat(line, "/");
                if (mon < 10) 
                    strcat(line, "0");
                itoa(tmp_num, mon);
                strcat(line, tmp_num); strcat(line, "/");
                itoa(tmp_num, y);
                strcat(line, tmp_num); strcat(line, "  ");
                // str_padding(out_buffer, strlen(out_buffer), 45); // On cale l'heure à 45
                str_padding(line, strlen(line), 45); // On cale l'heure à 45

                // Heure : HH:MM
                if (h < 10) 
                    strcat(line, "0");
                itoa(tmp_num, h);
                strcat(line, tmp_num); strcat(line, ":");
                if (min < 10) 
                    strcat(line, "0");
                itoa(tmp_num, min);
                strcat(line, tmp_num);
                
                strcat(line, "\n");
                strcat(out_buffer, line);
            } else {
                if (entries[i].attributes & 0x10) {
                    strcat(out_buffer, "["); strcat(out_buffer, clean_name); strcat(out_buffer, "] ");
                } else {
                    strcat(out_buffer, clean_name); strcat(out_buffer, "  ");
                }
            }
        }
        current_cluster = fat_get_next_cluster(current_cluster);
    }

    if (long_format) {
        char summary[128];
        summary[0] = '\0';
        strcat(summary, "\n---------------------------------------------------\n");
        
        itoa(tmp_num, file_count);
        strcat(summary, tmp_num);
        strcat(summary, " fichier(s), ");
        
        itoa(tmp_num, total_bytes);
        strcat(summary, tmp_num);
        strcat(summary, " octets au total.\n");
        
        strcat(out_buffer, summary);
    }

    #if DEBUG
        kprintf("DEBUG: Buffer final taille = %d\n", strlen(out_buffer));
    #endif

    return strlen(out_buffer);
}
*/

