#define _GNU_SOURCE

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"
#include "scheduler.h"
#include "serial_reader.h"
#include "server.h"
#include "time_utils.h"

pthread_t serial_reader_thread;
pthread_t log_queue_thread;

pthread_t data_manager_thread;

pthread_t server_thread;
pthread_t server_watchdog_thread;

Options *options;
Statistics statistics = { 0 };

void open_port() {
    if (serial_port_running) {
        printf("Serial port already running!\n");
        return;
    }
    if (serial_port_needs_join) {
        pthread_join(serial_reader_thread, NULL);
        serial_port_needs_join = false;
    }
    pthread_create(&serial_reader_thread, NULL, run_serial, options->serial->data);
}

void close_port() {
    if (serial_port_needs_join && !serial_port_running) {
        pthread_join(serial_reader_thread, NULL);
        serial_port_needs_join = false;
    }
    if (!serial_port_running) {
        printf("Serial port not running!\n");
        return;
    }
    pthread_cancel(serial_reader_thread);
    pthread_join(serial_reader_thread, NULL);
    serial_port_needs_join = false;
    serial_port_running = false;
    ZEJF_DEBUG(1, "joined with serial reader thread\n");
}

void open_server() {
    if (server_running) {
        printf("Server already running!\n");
        return;
    }
    if (server_needs_join) {
        pthread_join(server_thread, NULL);
        server_needs_join = false;
    }
    pthread_create(&server_thread, NULL, server_run, options);
}

void close_server() {
    if (server_needs_join && !server_running) {
        pthread_join(server_thread, NULL);
        server_needs_join = false;
    }
    if (!server_running) {
        printf("Server not running!\n");
        return;
    }
    pthread_cancel(server_thread);
    pthread_join(server_thread, NULL);
    server_close();
    server_needs_join = false;
    server_running = false;
    ZEJF_DEBUG(1, "Joined with server thread\n");
}

void print_info() {
    printf("\n========= ZejfCSeis v%s ===========\n", ZEJFCSEIS_VERSION);
    printf("sample rate: %d sps\n", SAMPLES_PER_SECOND);
    printf("serial port: %s\n", options->serial->data);
    printf("server address: %s:%d\n", options->ip_address->data, options->port);
    printf("\nserial port open: %d\n", serial_port_running);
    printf("server open: %d\n", server_running);
    printf("\nloaded datahours: %ld\n", datahours_count());
    printf("maximum queue length: %ld\n", statistics.queue_max_length);
    printf("gaps: %d\n", statistics.gaps);
    printf("arduino gaps: %d\n", statistics.arduino_gaps);
    printf("current serial port delay: %.3fms\n", last_avg_diff / 1000.0);
    printf("highest serial port delay: %.3fms\n", statistics.highest_avg_diff / 1000.0);
    printf("lowest serial port delay: %.3fms\n", statistics.lowest_avg_diff / 1000.0);
    printf("\nactive connections: %ld\n", client_count());
    printf("================================\n\n");
}

int64_t stress_count;
volatile bool stress_running = false;

void *stress() {
    stress_count = 0;
    int64_t start = (1663279200000l) / SAMPLE_TIME_MS;
    int64_t end = (1672009200000l + 1000 * 60 * 60 * 24l) / SAMPLE_TIME_MS;
    pthread_mutex_lock(&data_lock);
    while (start <= end) {
        int32_t hour_id = get_hour_id(start);
        printf("%d\n", hour_id);
        DataHour *dh = get_datahour(hour_id, true, false);
        if (dh != NULL) {
            dh->modified = true;
        }
        start += (1000 * 60 * 60) / SAMPLE_TIME_MS;
    }
    pthread_mutex_unlock(&data_lock);
    pthread_exit(0);
}


void print_help(void) {
    printf("\n====== Available commands =======\n");
    printf("help - show help\n");
    printf("exit - close ZejfCSeis\n");
    printf("info - print status and other technical info\n");
    printf("openport - try to open serial port\n");
    printf("closeport - close serial port\n");
    printf("openserver - try to open TCP server\n");
    printf("closeserver - close TCP server\n\n");
}

bool process_command(char *line) {
    if (strcmp(line, "exit\n") == 0) {
        return true;
    } else if (strcmp(line, "help\n") == 0) {
        print_help();
    } else if (strcmp(line, "info\n") == 0) {
        print_info();
    } else if (strcmp(line, "openport\n") == 0 || strcmp(line, "port\n") == 0) {
        open_port();
    } else if (strcmp(line, "closeport\n") == 0 || strcmp(line, "close\n") == 0) {
        close_port();
    } else if (strcmp(line, "server\n") == 0 || strcmp(line, "openserver\n") == 0) {
        open_server();
    } else if (strcmp(line, "closeserver\n") == 0) {
        close_server();
    } else if (strcmp(line, "stresstest\n") == 0) {
        printf("stress test, lock datahour for 1 seconds...\n");
        stress_running = true;
        pthread_t stress_thread;
        int64_t a = millis();
        pthread_create(&stress_thread, NULL, stress, NULL);
        pthread_join(stress_thread, NULL);
        printf("done: %ldms\n", millis() - a);
    } else if (strcmp(line, "resetstats\n") == 0) {
        statistics.arduino_gaps = 0;
        statistics.gaps = 0;
        statistics.highest_avg_diff = 0;
        statistics.lowest_avg_diff = 0;
        statistics.queue_max_length = 0;
        printf("statistics reset");
    } else {
        printf("Unknown command: %s", line);
    }
    return false;
}

void command_line() {
    ZEJF_DEBUG(0, "command line active\n");
    size_t len = 0;
    ssize_t lineSize = 0;
    while (true) {
        char *line = NULL;
        lineSize = getline(&line, &len, stdin);
        if (lineSize < 0) {
            free(line);
            break;
        }
        bool result = process_command(line);
        free(line);
        if (result) {
            break;
        }
    }
}

void run_threads(Options *x) {
    options = x;

    // init
    data_init();
    serial_init();
    server_init();

    // run stuff

    pthread_create(&log_queue_thread, NULL, run_queue_thread, NULL);
    open_port();
    pthread_create(&data_manager_thread, NULL, run_data_manager, NULL);
    pthread_create(&server_watchdog_thread, NULL, run_server_watchdog, NULL);
    open_server();

    command_line();

    zejfcseis_exit();
}

void zejfcseis_exit(void) {
    // order is important

    printf("Closing ZejfCSeis...\n");

    close_server();
    pthread_cancel(server_watchdog_thread);
    pthread_join(server_watchdog_thread, NULL);
    ZEJF_DEBUG(0, "joined with server watchdog thread\n");

    server_destroy();
    ZEJF_DEBUG(0, "server destroyed.\n");
    close_port();
    ZEJF_DEBUG(0, "port closed\n");
    queue_thread_end();
    ZEJF_DEBUG(0, "queue Thread end sent\n");

    pthread_cancel(log_queue_thread);
    pthread_join(log_queue_thread, NULL);
    ZEJF_DEBUG(0, "joined with queue thread\n");

    pthread_cancel(data_manager_thread);
    pthread_join(data_manager_thread, NULL);

    serial_reader_destroy();
    ZEJF_DEBUG(0, "serial reader destroyed\n");

    data_destroy();
    ZEJF_DEBUG(0, "joined with data manager thread\n");
}