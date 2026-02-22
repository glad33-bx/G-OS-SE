#ifndef STRING_H
#define STRING_H

#include "terminal.h"
#include "gos_types.h"


int str_equal(char *s1, char *s2);
int str_starts_with(const char* str, const char* prefix) ;
int str_compare(char *s1, char *s2);
int strcmp(const char *s1, const char *s2);
uint32_t strlen(const char *s);
char* strcpy(char* dest, const char* src) ;
char* strncpy(char* dest, const char* src, uint32_t n);
int memcmp(const void *s1, const void *s2, uint32_t n);
void trim(char* str) ;
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
char *strchr(const char *s, int c);
void kprint_hex(uint32_t num);
void itoa_hex(char* dest, uint32_t num);
void itoa(char* dest, uint32_t n);
char tolower(char c);
char toupper(char c);


#endif