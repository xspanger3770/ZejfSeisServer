#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#define CLIENT_TIMEOUT_SEC 20
#define REALTIME_MAX_GAP_MINUTES 5

#define DATA_REQUEST_BUFFER 128
#define DATA_REQUEST_MAX_LENGTH_HOURS 24
#define DATA_REQUEST_CHUNK_SIZE_MINUTES 4

extern volatile bool server_running;
extern volatile bool server_needs_join;

typedef struct datarequest_t{
    int64_t first_log_id;
    int64_t last_log_id;
}DataRequest;

typedef struct serverclient_t{
    int socket;
    bool connected;
    bool realtime;
    bool heartbeat_request;
    FILE* file;
    pthread_t input_thread;
    pthread_t output_thread;

    sem_t output_semaphore;

    int64_t last_sent_log_id;

    size_t id;
    int64_t last_heartbeat;

    pthread_mutex_t data_requests_mutex;
    int requests_head;
    int requests_tail;
    DataRequest data_requests[DATA_REQUEST_BUFFER];
}ServerClient;


void server_init();

void* server_run(void* arg);

void* run_server_watchdog();

void server_realtime_notify(void);

void server_close(void);

void server_destroy();

size_t client_count(void);

#endif