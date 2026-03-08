#ifndef CMOS_H
#define CMOS_H

#include "../lib/gos_types.h"
#include "io.h"
// #include "kernel.h"

extern int timezone_offset;

uint8_t read_cmos(uint8_t reg);
uint16_t get_fat_time_rtc();
uint16_t get_fat_date_rtc();
uint16_t get_fat_time_rtc() ;


#endif