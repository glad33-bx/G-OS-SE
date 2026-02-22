#ifndef RTC_H
#define RTC_H

#include "../lib/gos_types.h"

extern int timezone_offset;

void get_current_datetime(int *day, int *month, int *year, int *hour, int *min, int *sec);

#endif