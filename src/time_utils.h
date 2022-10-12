#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


int64_t millis(void);

int64_t micros(void);

int32_t hours(void);

int32_t get_hour_id(int64_t log_id);

int64_t get_first_log_id(int32_t hour_id);

#endif