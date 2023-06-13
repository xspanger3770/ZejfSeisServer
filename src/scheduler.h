#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "my_string.h"

#define ZEJF_VERSION "1.5.1"
#define COMPATIBILITY_VERSION 4

// DEBUG
#define ZEJF_LOG_DEBUG 0
#define ZEJF_LOG_INFO 1
#define ZEJF_LOG_CRITICAL 2

#define ZEJF_LOG_LEVEL 1
#define ZEJF_LOG(p, x, ...)  \
    do { if(p >= ZEJF_LOG_LEVEL) printf(x, ##__VA_ARGS__); } while(0)


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

void zejf_exit(void);

#endif