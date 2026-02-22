#ifndef SHELL_H
#define SHELL_H

#include "../lib/gos_types.h"
#include "../fs/fs.h"
#include "../lib/string.h"
#include "../lib/config.h"
#include "../kernel/terminal.h"
#include "../kernel/gos_memory.h"
#include "../kernel/io.h"
#include "../kernel/keyboard.h"

#define PROMPT_STR_VISIBLE_SIZE 2

// Point d'entrée pour interpréter une ligne de commande
void interpret_command(char* buffer);

// Utilitaires de parsing (si tu ne les as pas ailleurs)
void trim(char* str);
int str_starts_with(const char* str, const char* prefix);
void process_single_command(char* buffer);
uint32_t get_esp();
void command_info();
void force_exit();
void shell_more(char* filename);

#endif