#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

#include <errno.h>

#include "arraylist.h"
#include "com_utils.h"
#include "data.h"
#include "my_string.h"
#include "scheduler.h"
#include "serial_reader.h"
#include "server.h"
#include "time_utils.h"

volatile bool server_running = false;
volatile bool server_needs_join = false;

ArrayList *clients = NULL;
size_t next_client_id;

pthread_mutex_t clients_lock;

void server_init() {
    clients = list_create(sizeof(ServerClient *));
    pthread_mutex_init(&clients_lock, NULL);
    signal(SIGPIPE, SIG_IGN);
}

void register_request(ServerClient *client, int64_t first_log_id, int64_t last_log_id) {
    ZEJF_DEBUG(0, "registering DataRequest from %ld to %ld\n", first_log_id, last_log_id);
    if (last_log_id < first_log_id) {
        ZEJF_DEBUG(1, "invalid request\n");
        return;
    }

    if ((last_log_id - first_log_id) * SAMPLE_TIME_MS / (1000 * 60 * 60l) > DATA_REQUEST_MAX_LENGTH_HOURS) {
        ZEJF_DEBUG(1, "too long request\n");
        return;
    }

    pthread_mutex_lock(&client->data_requests_mutex);

    if ((client->requests_head + 1) % DATA_REQUEST_BUFFER == client->requests_tail) {
        pthread_mutex_unlock(&client->data_requests_mutex);
        ZEJF_DEBUG(1, "ERROR: maximum number of DataRequests reached for client #%ld\n", client->id);
        return;
    }

    ZEJF_DEBUG(0, "HEAD %d, TAIL %d, MAX = %d\n", client->requests_head, client->requests_tail, DATA_REQUEST_BUFFER);

    client->data_requests[client->requests_head].first_log_id = first_log_id;
    client->data_requests[client->requests_head].last_log_id = last_log_id;
    client->requests_head = (client->requests_head + 1) % DATA_REQUEST_BUFFER;
    pthread_mutex_unlock(&client->data_requests_mutex);

    sem_post(&client->output_semaphore);
}

void process_client_command(ServerClient *client, char *command) {
    if (strcmp(command, "realtime\n") == 0) {
        int64_t last_log_id = read_64(client->file);
        client->last_sent_log_id = last_log_id;
        client->realtime = !client->realtime;
        ZEJF_DEBUG(0, "realtime toggled for client #%ld from %ld\n", client->id, last_log_id);
    } else if (strcmp(command, "getdata\n") == 0) {
        int64_t first_log_id = read_64(client->file);
        int64_t last_log_id = read_64(client->file);
        register_request(client, first_log_id, last_log_id);
    } else if (strcmp(command, "heartbeat\n") == 0) {
        client->last_heartbeat = millis();
    } else if (strcmp(command, "datahour_check\n") == 0) {
        int32_t hour_id = (int32_t) read_64(client->file);
        int64_t sample_count = read_64(client->file);
        pthread_mutex_lock(&data_lock);
        DataHour *dh = get_datahour(hour_id, true, false);
        pthread_mutex_unlock(&data_lock);
        if (dh != NULL && dh->sample_count != sample_count) {
            register_request(client, get_first_log_id(hour_id), get_first_log_id(hour_id + 1) - 1);
        }
    } else if (strcmp(command, "senddata\n") == 0) {
        int32_t value = (int32_t) read_64(client->file);
        int64_t log_id = read_64(client->file);

        pthread_mutex_lock(&log_queue_lock);
        next_log(value, log_id);
        pthread_mutex_unlock(&log_queue_lock);

        sem_post(&log_queue_semaphore);
    } else {
        ZEJF_DEBUG(1, "client #%ld received unknown command '%s'\n", client->id, command);
    }
}

#define COMMAND_BUFFER_SIZE 128

void *run_input_thread(void *arg) {
    ServerClient *client = (ServerClient *) arg;

    char buffer[COMMAND_BUFFER_SIZE];
    while (fgets(buffer, COMMAND_BUFFER_SIZE, client->file) != NULL) {
        process_client_command(client, buffer);
    }

    ZEJF_DEBUG(0, "client #%ld input thread finish\n", client->id);
    client->connected = false;
    pthread_exit(0);
}

#define SEND_BUFFER_SIZE 2048

bool send_logs(int fd, int64_t start, int64_t end, int64_t *last_ptr, char *command) {
    int64_t count = (end - start) + 1;

    if (write(fd, command, strlen(command)) <= 0) {
        return false;
    }

    char send_buffer[SEND_BUFFER_SIZE];
    char *send_buffer_ptr;
    int sn_count;

    while (count > 0) {
        send_buffer_ptr = send_buffer;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&data_lock);
        while (count > 0) {
            int32_t val = get_log(start);
            sn_count = 0;
            if (val != ERR_VAL) {
                sn_count = snprintf(send_buffer_ptr, 48, "%d\n%ld\n", val, start);
                if (sn_count < 0) {
                    ZEJF_DEBUG(1, "snprintf fail\n");
                    pthread_mutex_unlock(&data_lock);
                    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                    return false;
                }
            }

            *last_ptr = start;
            start++;
            count--;
            send_buffer_ptr += sn_count;
            if ((send_buffer_ptr - send_buffer) >= SEND_BUFFER_SIZE - 48 || count == 0) {
                break;
            }
        }

        pthread_mutex_unlock(&data_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        if (send_buffer_ptr - send_buffer == 0) {
            continue;
        }

        if (write(fd, send_buffer, send_buffer_ptr - send_buffer) <= 0) {
            perror("write");
            return false;
        }
    }

    char msg[32];

    sn_count = snprintf(msg, 32, "%d\n", ERR_VAL);
    if (sn_count < 0) {
        ZEJF_DEBUG(1, "snprintf fail\n");
        return false;
    }

    if (write(fd, msg, strlen(msg)) <= 0) {
        perror("write");
        return false;
    }

    return true;
}

bool send_realtime(ServerClient *client) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&data_lock);
    int64_t last_log = last_received_log_id;
    pthread_mutex_unlock(&data_lock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    if (client->last_sent_log_id == last_log) {
        return true;
    }

    if (client->last_sent_log_id > last_log) {
        return true;
    }

    if (last_log - client->last_sent_log_id > (REALTIME_MAX_GAP_MINUTES * 60 * 1000) / SAMPLE_TIME_MS) {
        client->last_sent_log_id = last_log - 1;
    }

    send_logs(client->socket, client->last_sent_log_id + 1, last_log, &client->last_sent_log_id, "realtime\n");

    return true;
}

bool send_requests(ServerClient *client) {
    int head, tail;
    pthread_mutex_lock(&client->data_requests_mutex);
    head = client->requests_head;
    tail = client->requests_tail;
    pthread_mutex_unlock(&client->data_requests_mutex);

    int64_t sent = 0;

    while (tail != head) {
        DataRequest *request = &(client->data_requests[tail]);
        int64_t count = request->last_log_id - request->first_log_id + 1;
        count = MIN(count, (DATA_REQUEST_CHUNK_SIZE_MINUTES * 60 * 1000) / SAMPLE_TIME_MS - sent);
        sent += count;

        send_logs(client->socket, request->first_log_id, request->first_log_id + count - 1, &request->first_log_id, "logs\n");

        if (request->first_log_id >= request->last_log_id) {
            tail++;
            tail %= DATA_REQUEST_BUFFER;
        } else {
            sem_post(&client->output_semaphore);
            break;
        }
    }

    pthread_mutex_lock(&client->data_requests_mutex);
    client->requests_tail = tail;
    pthread_mutex_unlock(&client->data_requests_mutex);

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 2 * 1000;
    nanosleep(&ts, NULL);

    return true;
}

bool send_heartbeat(ServerClient *client) {
    client->heartbeat_request = false;
    if (write(client->socket, "heartbeat\n", 10) == -1) {
        perror("write");
        return false;
    }

    return true;
}

void *run_output_thread(void *arg) {
    ServerClient *client = (ServerClient *) arg;

    while (client->connected) {
        sem_wait(&client->output_semaphore);

        if (!client->connected) {
            break;
        }

        if (client->heartbeat_request && !send_heartbeat(client)) {
            break;
        }

        if (client->realtime && !send_realtime(client)) {
            break;
        }

        if (!send_requests(client)) {
            break;
        }
    }

    ZEJF_DEBUG(0, "client #%ld output thread finish\n", client->id);
    client->connected = false;

    pthread_exit(0);
}

void server_realtime_notify(void) {
    pthread_mutex_lock(&clients_lock);
    if (clients == NULL) {
        pthread_mutex_unlock(&clients_lock);
        return;
    }
    for (size_t i = 0; i < clients->item_count; i++) {
        ServerClient *client = *(ServerClient **) list_get(clients, i);
        if (client->realtime) {
            sem_post(&client->output_semaphore);
        }
    }

    pthread_mutex_unlock(&clients_lock);
}

bool send_initial_info(int socket) {
    char msg[4][32];
    snprintf(msg[0], 32, "compatibility_version:%d\n", COMPATIBILITY_VERSION);
    snprintf(msg[1], 32, "sample_rate:%d\n", SAMPLES_PER_SECOND);
    snprintf(msg[2], 32, "err_value:%d\n", ERR_VAL);
    snprintf(msg[3], 32, "last_log_id:%ld\n", last_received_log_id);

    for (int i = 0; i < 4; i++) {
        if (!write(socket, msg[i], strlen(msg[i]))) {
            perror("write");
            return false;
        }
    }

    ZEJF_DEBUG(0, "Initial info sent.\n");
    return true;
}

void client_connect(int socket) {
    if (!send_initial_info(socket)) {
        return;
    }
    ServerClient *client = malloc(sizeof(ServerClient));
    if (client == NULL) {
        perror("malloc");
        return;
    }
    client->connected = true;
    client->socket = socket;
    client->file = fdopen(socket, "r");
    client->realtime = false;
    client->id = next_client_id++;
    client->last_heartbeat = millis();
    client->last_sent_log_id = -1;

    client->heartbeat_request = false;

    client->requests_head = 0;
    client->requests_tail = 0;

    sem_init(&client->output_semaphore, 0, 0);

    pthread_mutex_init(&client->data_requests_mutex, NULL);

    pthread_create(&client->input_thread, NULL, run_input_thread, client);
    pthread_create(&client->output_thread, NULL, run_output_thread, client);

    pthread_mutex_lock(&clients_lock);
    list_append(clients, &client);
    pthread_mutex_unlock(&clients_lock);

    ZEJF_DEBUG(0, "current client count: %ld\n", clients->item_count);
}

void client_destructor(void **ptr) {
    if (ptr == NULL) {
        return;
    }
    ServerClient *client = *((ServerClient **) ptr);
    ZEJF_DEBUG(0, "destroying client #%ld\n", client->id);
    client->connected = false;

    sem_post(&client->output_semaphore);

    pthread_cancel(client->input_thread);
    pthread_join(client->input_thread, NULL);
    pthread_cancel(client->output_thread);
    pthread_join(client->output_thread, NULL);

    sem_destroy(&client->output_semaphore);

    fclose(client->file);
    client->file = NULL;

    ZEJF_DEBUG(0, "done destroying client #%ld\n", client->id);
    free(client);
}

int server_fd;

void *server_run(void *args) {
    server_needs_join = true;
    server_running = true;
    next_client_id = 0;
    Options *options = (Options *) args;
    printf("Opening server... %s:%d\n", options->ip_address->data, options->port);

    int new_socket;
    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        printf("Unable to open server: %s\n", strerror(errno));
        perror("socket failed");
        server_running = false;
        pthread_exit(0);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("Unable to open server: %s\n", strerror(errno));
        perror("setsockopt");
        server_running = false;
        pthread_exit(0);
    }

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(options->ip_address->data);
    address.sin_port = htons(options->port);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        printf("Unable to bind server: %s\n", strerror(errno));
        perror("bind failed");
        server_running = false;
        pthread_exit(0);
    }

    printf("Server is open!\n");

    while (true) {
        if (listen(server_fd, 3) < 0) {
            printf("Server closed: %s\n", strerror(errno));
            perror("listen");
            server_running = false;
            pthread_exit(0);
        }
        ZEJF_DEBUG(0, "accept\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            printf("Server closed: %s\n", strerror(errno));
            perror("accept");
            server_running = false;
            pthread_exit(0);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        client_connect(new_socket);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    server_running = false;
    pthread_exit(0);
}

void *run_server_watchdog() {
    while (true) {
        sleep(2);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&clients_lock);
        size_t i = 0;
        int64_t time = millis();
        while (i < clients->item_count) {
            ServerClient *client = *(ServerClient **) list_get(clients, i);
            if (!client->connected || time - client->last_heartbeat > CLIENT_TIMEOUT_SEC * 1000) {
                ZEJF_DEBUG(0, "client #%ld timeout\n", client->id);
                list_remove(clients, i, client_destructor);
                i--;
            } else {
                client->heartbeat_request = true;
                sem_post(&client->output_semaphore);
            }
            i++;
        }
        pthread_mutex_unlock(&clients_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    pthread_exit(0);
}

size_t client_count(void) {
    if (clients == NULL) {
        return 0;
    }
    return clients->item_count;
}

void server_close(void) {
    if (shutdown(server_fd, SHUT_RDWR) == -1) {
        perror("shutdown");
    }
}

void server_destroy(void) {
    pthread_mutex_lock(&clients_lock);
    list_destroy(clients, client_destructor);
    clients = NULL;
    pthread_mutex_unlock(&clients_lock);
}