#ifndef DATA_H
#define DATA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <pthread.h>

#include "my_string.h"

extern const int SAMPLE_RATES[5];
extern int SAMPLES_PER_SECOND;
extern int SAMPLE_TIME_MS;
extern int SAMPLES_IN_HOUR;

#define ERR_VAL -2147483647

#define PERMANENTLY_LOADED_HOURS 12
#define STORE_TIME_MINUTES 5

#define MAIN_FOLDER "./ZejfCSeis/"

extern pthread_mutex_t data_lock;

extern int64_t last_received_log_id;

typedef struct datahour_t
{
    bool modified;
    int64_t last_access_ms;
    int32_t hour_id;
    int sample_count;
    int32_t samples[];
} DataHour;

size_t datahour_get_size();

DataHour *datahour_create(int32_t hour_id);

void datahour_destroy(DataHour *datahour);

bool datahour_save(DataHour *dh);

int32_t get_log(int64_t log_id);

void log_data(int64_t log_id, int32_t val);

void data_init(void);

void data_destroy(void);

String *get_datahour_path_new(int32_t hour_id);

String *get_datahour_path_newest(int32_t hour_id);

String *get_datahour_path_old(int32_t hour_id);

DataHour *get_datahour(int32_t hour_id, bool load_from_file, bool create_new);

void *run_data_manager();

size_t datahours_count();

#endif