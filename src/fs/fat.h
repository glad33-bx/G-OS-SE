#include "../lib/gos_types.h"

extern uint8_t fat_tmp_buffer[512];
extern uint32_t fs_status;
extern uint32_t current_dir_cluster;

/*
La structure du secteur de boot (BPB)
BPB FAT16 (souvent appelé BIOS Parameter Block étendu).
Le premier secteur (LBA 0) du disque n'est plus vide. 
Il doit contenir le BIOS Parameter Block. 
C'est là que l'OS stocke la "carte d'identité" du disque.
*/
/*struct fat_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors_short;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;     // <--- Vérifie que celui-ci est bien présent !
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;

    // Champs spécifiques FAT16 (Extended BPB)
    uint32_t sectors_per_fat32;   // Offset 36 (parfois utilisé par mformat)
    uint16_t ext_flags;
} __attribute__((packed));*/

/*struct fat_bpb {
    uint8_t  jmp[3];                 // Saut vers le code de boot
    char     oem[8];                 // Nom de l'outil de formatage (ex: "mtools")
    uint16_t bytes_per_sector;       // Généralement 512
    uint8_t  sectors_per_cluster;    // Puissance de 2 (1, 2, 4, 8...)
    uint16_t reserved_sectors;       // Offset 14 (Tes fameux 32 secteurs)
    uint8_t  fats;                   // Nombre de FATs (Généralement 2)
    uint16_t root_dir_entries;       // Offset 17 (Généralement 512 en FAT16)
    uint16_t total_sectors_short;    // Si 0, utiliser total_sectors_large
    uint8_t  media_descriptor;       // 0xF8 pour un disque dur
    uint16_t sectors_per_fat;        // Offset 22 (Taille de la FAT en secteurs)
    uint16_t sectors_per_track;      // Géométrie CHS
    uint16_t heads;                  // Géométrie CHS
    uint32_t hidden_sectors;         // Secteurs avant la partition
    uint32_t total_sectors_large;    // Nombre total de secteurs si > 65535

    // --- Section Étendue FAT16 (Commence à l'offset 36) ---
    uint8_t  drive_number;           // 0x80 pour le premier disque dur
    uint8_t  reserved_byte;          // Réservé (0)
    uint8_t  signature;              // 0x28 ou 0x29 (Extended Boot Signature)
    uint32_t volume_id;              // Numéro de série du volume
    char     volume_label[11];       // Nom du disque ("NO NAME    ")
    char     system_id[8];           // Système de fichiers ("FAT16   ")
} __attribute__((packed));*/
/*#pragma pack(push, 1)
struct fat_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors_short;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_large;
    // Section FAT16
    uint8_t  drive_number;
    uint8_t  reserved_byte;
    uint8_t  signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     system_id[8];
};
#pragma pack(pop)*/

/*struct fat32_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_dir_entries;
    uint32_t total_sectors_large;
    
    // Champs FAT32 (Offset 36)
    uint32_t sectors_per_fat32;    // Ce fameux 1015 que tu as vu !
    uint16_t extended_flags;
    uint16_t fs_version;
    uint32_t root_cluster;         // <--- TRÈS IMPORTANT (souvent 2)
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  signature;            // 0x29
    uint32_t volume_id;
    char     volume_label[11];
    char     system_id[8];         // "FAT32   "
} __attribute__((packed));*/

struct fat32_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_dir_entries; // Souvent 0 en FAT32
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat16; // 0 en FAT32
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // Champs spécifiques FAT32 (Offset 36+)
    uint32_t sectors_per_fat32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster; // Offset 44
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed));

// Chaque fichier dans le Root Directory (secteur 132) occupe 32 octets.
struct fat_directory_entry {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high; // Toujours 0 en FAT16
    uint16_t last_modification_time;
    uint16_t last_modification_date;
    uint16_t first_cluster_low;  // <--- C'est lui qu'on veut !
    uint32_t file_size;
} __attribute__((packed));



#pragma pack(push, 1)
struct fat_dir_entry {
    char     name[8];          // Nom (complété par des espaces)
    char     ext[3];           // Extension (complétée par des espaces)
    uint8_t  attributes;       // Attributs (0x0F pour LFN, 0x10 pour dossier, etc.)
    uint8_t  reserved;         // Réservé pour Windows NT
    uint8_t  creation_time_ms; // Temps de création en ms
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;     // Partie haute du cluster (Spécifique FAT32)
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t cluster_low;      // Partie basse du cluster
    uint32_t size;             // Taille du fichier en octets
};
#pragma pack(pop)/*
Pour lire un fichier sur FAT16, GillesOS devra suivre ce chemin :
    Lire le BPB (Secteur 0) pour savoir où commence la FAT et où commence le répertoire racine.
    Chercher dans le Root Directory le nom du fichier (ex: KERNEL BIN).
    Récupérer le premier Cluster du fichier.
    Consulter la FAT pour savoir quel est le cluster suivant (si le fichier est gros).

Le calcul du Secteur de Données
C'est là que les maths de ton mkfs.vfat deviennent utiles. Un fichier 
commence à un numéro de Cluster. En FAT16, le Cluster 2 est le tout premier cluster de données.

D'après tes logs, la zone de données commence au secteur 164. La formule magique est :
Secteur=164+(Cluster−2)×SecteursParCluster
*/

void fat_init();

//void fat_list_root();
void fat_ls();

uint32_t fat_find_file_cluster(const char* filename);
uint32_t fat_find_free_cluster();
uint32_t fat_get_next_cluster(uint32_t cluster);
uint32_t fat_cluster_to_lba(uint32_t cluster);
void fat_read_cluster(uint32_t cluster, uint8_t* buffer);
void fat_write_cluster(uint32_t cluster, uint8_t* buffer);
void fat_set_cluster_value(uint32_t cluster, uint32_t value);

void to_fat_name(const char* input, char* output);
uint32_t get_fat_timestamp();

void* fat_get_file_content(uint32_t cluster);
void load_file(uint32_t start_cluster, uint8_t* destination);
void fat_overwrite_file_content(const char* filename, const char* new_text);
void fat_overwrite_file(const char* filename, const char* text) ;
void fat_remove_file(const char* filename);
void fat_copy_file(const char* src_name, const char* dest_name);
int fat_read_file(char* name, uint8_t* buffer);
void fat_cat(const char* filename);
void fat_touch(const char* filename);
void fat_echo(const char* text, const char* filename);

int fat_mkdir(char* name);
int is_directory(char* name);
int fat_get_entry(char* fat_name, uint8_t* buffer_out);
int fat_cd(char* path) ;

void fat_debug_root_dump();
void shell_loop();


