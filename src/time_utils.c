#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "data.h"
#include "time_utils.h"

int64_t millis(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000;
    int64_t s2 = (time.tv_usec / 1000);
    return s1 + s2;
}

int64_t micros(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000000;
    int64_t s2 = (time.tv_usec);
    return s1 + s2;
}

int32_t hours(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int32_t)(time.tv_sec / (60 * 60));
}

int32_t get_hour_id(int64_t log_id)
{
    return (log_id * (SAMPLE_TIME_MS)) / (1000 * 60 * 60);
}

int64_t get_first_log_id(int32_t hour_id)
{
    return (hour_id * 1000l * 60 * 60) / SAMPLE_TIME_MS;
}