#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "my_string.h"

#define ZEJFCSEIS_VERSION "1.5.0"
#define COMPATIBILITY_VERSION 4

#define ZEJF_DEBUG_NORMAL 0
#define ZEJF_DEBUG_IMPORTANT 1
#define ZEJF_DEBUG_CRITICAL 2
#define ZEJF_DEBUG_DISABLED 99

#define ZEJF_DEBUG_LEVEL 99
#define ZEJF_DEBUG(p, x, ...)  \
    do { if(p >= ZEJF_DEBUG_LEVEL) printf(x, ##__VA_ARGS__); } while(0)


typedef struct options_t
{
    String *serial;
    String *ip_address;
    int port;
    int sample_rate_id;
} Options;

typedef struct statistics_t
{
    size_t queue_max_length;
    int gaps;
    int arduino_gaps;
    double highest_avg_diff;
    double lowest_avg_diff;
} Statistics;

extern Statistics statistics;
extern Options *options;

void run_threads(Options *opts);

void zejfcseis_exit(void);

#endif