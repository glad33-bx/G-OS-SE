#include "cmos.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

// Fonction de conversion BCD -> Décimal
static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

/*uint16_t get_fat_time_rtc() {
    // Note: Les valeurs CMOS sont souvent en BCD (Binary Coded Decimal)
    // Il faut les convertir : (val / 16) * 10 + (val % 16)
    uint8_t second = read_cmos(0x00);
    uint8_t minute = read_cmos(0x02);
    uint8_t hour   = read_cmos(0x04);

    // Conversion BCD -> Decimal (si nécessaire selon ton émulateur)
    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour   = (hour   & 0x0F) + ((hour   / 16) * 10);

    return (uint16_t)((hour << 11) | (minute << 5) | (second / 2));
}*/

uint16_t get_fat_time_rtc() {
    uint8_t second = bcd_to_bin(read_cmos(0x00));
    uint8_t minute = bcd_to_bin(read_cmos(0x02));
    uint8_t hour   = bcd_to_bin(read_cmos(0x04));

    // Application du fuseau horaire dynamique
    int local_hour = (int)hour + timezone_offset;

    // Gestion sommaire du débordement (0-23h)
    if (local_hour >= 24) local_hour -= 24;
    if (local_hour < 0)   local_hour += 24;

    return (uint16_t)(((uint8_t)local_hour << 11) | (minute << 5) | (second / 2));
}


// Récupération de la date au format FAT (16 bits)
uint16_t get_fat_date_rtc() {
    uint8_t day   = bcd_to_bin(read_cmos(0x07));
    uint8_t month = bcd_to_bin(read_cmos(0x08));
    uint16_t year = bcd_to_bin(read_cmos(0x09)) + 2000;

    return (uint16_t)(((year - 1980) << 9) | (month << 5) | day);
}





