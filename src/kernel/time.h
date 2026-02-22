#include "../lib/gos_types.h"

// Compresse l'heure au format FAT
uint16_t encode_fat_time(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    return (uint16_t)(((hours & 0x1F) << 11) | ((minutes & 0x3F) << 5) | ((seconds / 2) & 0x1F));
}

// Compresse la date au format FAT
uint16_t encode_fat_date(uint16_t year, uint8_t month, uint8_t day) {
    if (year < 1980) year = 1980;
    return (uint16_t)((((year - 1980) & 0x7F) << 9) | ((month & 0x0F) << 5) | (day & 0x1F));
}