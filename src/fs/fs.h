#ifndef FS_H
#define FS_H

#include "../lib/gos_types.h"
#include "../kernel/mem.h"
#include "../kernel/terminal.h"
#include "../lib/string.h"
#include "../kernel/gos_memory.h"
#include "../lib/config.h"

#define MAX_FILES 32
#define MAX_FILENAME_LEN 32
#define MAX_FILENAME 32
#define FILE_BUFFER_SIZE 4096 // Un fichier = 1 page pour l'instant

typedef struct {
    char name[MAX_FILENAME];
    uint8_t* data;
    uint32_t size;
    uint8_t active; // 0 = vide, 1 = utilisé
} vfile_t;

int init_fs();
int create_file(char* name) ;
void list_files();
int write_file(char* name, char* content);
void read_file(char* name);
int delete_file(char* name);
char* find_file_by_prefix(const char* prefix);
int find_file_idx(const char* name);
int get_free_slot() ;
uint8_t* get_file_data(int idx);;
int is_valid_name(char* name);

#endif