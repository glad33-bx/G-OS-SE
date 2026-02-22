#include "rtc.h"
#include "io.h" 

int timezone_offset = 0; // Par défaut GMT

static uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int is_update_in_progress() {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

// C'est la SEULE fonction publique
void get_current_datetime(int *day, int *month, int *year, int *hour, int *min, int *sec) {
    while (is_update_in_progress());

    *sec   = bcd_to_bin(get_rtc_register(0x00));
    *min   = bcd_to_bin(get_rtc_register(0x02));
    int raw_hour = bcd_to_bin(get_rtc_register(0x04));
    *day   = bcd_to_bin(get_rtc_register(0x07));
    *month = bcd_to_bin(get_rtc_register(0x08));
    *year  = bcd_to_bin(get_rtc_register(0x09)) + 2000;

    // Application du fuseau horaire
    int corrected_hour = raw_hour + timezone_offset;

    if (corrected_hour >= 24) {
        corrected_hour -= 24;
        (*day)++; // /!\ Attention : simplification ici, ne gère pas la fin de mois
    } else if (corrected_hour < 0) {
        corrected_hour += 24;
        (*day)--;
    }

    *hour = corrected_hour;
}