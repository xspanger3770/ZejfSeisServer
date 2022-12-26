#ifndef SERIAL_READER_H
#define SERIAL_READER_H

#define LOG_QUEUE_SIZE 12000

#define IGNORE_FIRST 50

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <semaphore.h>

extern volatile bool serial_port_running;
extern volatile bool serial_port_needs_join;

extern double last_avg_diff;

extern sem_t log_queue_semaphore;
extern pthread_mutex_t log_queue_lock;

typedef struct log_t
{
    int64_t log_id;
    int32_t val;
} Log;

typedef struct log_queue_t
{
    size_t head;
    size_t tail;
    Log logs[LOG_QUEUE_SIZE];
} LogQueue;

extern LogQueue *log_queue;

int serial_init(void);

int run_serial(char *serial);

void *run_queue_thread();

void queue_thread_end(void);

void serial_reader_destroy(void);

void next_log(int32_t value, int64_t log_id);

#endif