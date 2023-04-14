#define _GNU_SOURCE

// C library headers
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Linux headers
#include <errno.h> // Error integer and strerror() function
#include <fcntl.h> // Contains file controls like O_RDWR
#include <stdint.h>
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()

#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "data.h"
#include "scheduler.h"
#include "serial_reader.h"
#include "server.h"
#include "time_utils.h"

#define CALIBRATION_TRESHOLD 1500

LogQueue *log_queue;
pthread_mutex_t log_queue_lock;

sem_t log_queue_semaphore;

volatile bool queue_thread_running = false;
volatile bool serial_port_running = false;
volatile bool serial_port_needs_join = false;

int serial_port = -1;

int serial_init(void) {
    log_queue = calloc(1, sizeof(LogQueue));
    if (log_queue == NULL) {
        return 1;
    }

    pthread_mutex_init(&log_queue_lock, NULL);
    sem_init(&log_queue_semaphore, 0, 0);

    return 0;
}

int64_t last_log_id = -1;

void next_log(int32_t value, int64_t log_id) {
    if (last_log_id == -1) {
        last_log_id = log_id - 1;
    }
    int64_t gap = log_id - last_log_id;
    if (gap > 1) {
        ZEJF_DEBUG(2, "GAP %ld!\n", gap);
        statistics.gaps++;
    }

    last_log_id = log_id;

    log_queue->logs[log_queue->head].log_id = log_id;
    log_queue->logs[log_queue->head].val = value;

    log_queue->head++;
    log_queue->head %= LOG_QUEUE_SIZE;

    if (log_queue->head == log_queue->tail) {
        log_queue->tail++;
        //TODO FATAL queue overflow
    }
}

void *run_queue_thread() {
    ZEJF_DEBUG(0, "QueueThread run\n");
    queue_thread_running = true;
    while (queue_thread_running) {
        sem_wait(&log_queue_semaphore);

        if (!queue_thread_running) {
            break;
        }

        pthread_mutex_lock(&log_queue_lock);

        size_t head = log_queue->head;
        size_t tail = log_queue->tail;

        pthread_mutex_unlock(&log_queue_lock);

        if (head == tail) {
            continue;
        }

        size_t queue_length = 0;

        if (head >= tail) {
            queue_length = head - tail;
        } else {
            queue_length = head + (LOG_QUEUE_SIZE - tail);
        }

        if (queue_length > statistics.queue_max_length) {
            statistics.queue_max_length = queue_length;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&data_lock);

        while (tail != head) {
            log_data(log_queue->logs[tail].log_id, log_queue->logs[tail].val);
            tail++;
            tail %= LOG_QUEUE_SIZE;
        }

        pthread_mutex_unlock(&data_lock);
        server_realtime_notify();

        pthread_mutex_lock(&log_queue_lock);
        log_queue->tail = tail;
        pthread_mutex_unlock(&log_queue_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    ZEJF_DEBUG(0, "QueueThread finish\n");
    pthread_exit(0);
}

void queue_thread_end(void) {
    queue_thread_running = false;
    sem_post(&log_queue_semaphore);
}

unsigned int count_diffs = 0;
int64_t sum_diffs = 0;
double last_avg_diff = 0;
bool last_set = false;
bool calibrating = false;

#define SHIFT_CHECK 80

const char plus = '+';
const char minus = '-';
const char star = '*';

void diff_control(int64_t diff, int shift) {
    count_diffs++;
    sum_diffs += diff;
    if (count_diffs == SHIFT_CHECK) {
        double avg_diff = sum_diffs / (double) count_diffs;
        count_diffs = 0;
        sum_diffs = 0;

        if (last_set) {
            if (calibrating && fabs(avg_diff) < CALIBRATION_TRESHOLD) {
                calibrating = false;
                printf("Calibration done, you can now see the data.\n");
            }

            double change = avg_diff - last_avg_diff;
            double goal = calibrating ? -avg_diff / 4.0 : -avg_diff / 5.0;
            int shift_goal = (shift + (goal - change) / SHIFT_CHECK);
            int conf = (shift_goal - shift);

            if (calibrating) {
                conf *= 1.50;
            }

            if (conf > 0) {
                for (int i = 0; i < conf / 3 + 1; i++) {
                    if (write(serial_port, &plus, 1) == -1) {
                        perror("write");
                        return;
                    }
                }
            }
            if (conf < 0) {
                for (int i = 0; i < -conf / 3 + 1; i++) {
                    if (write(serial_port, &minus, 1) == -1) {
                        perror("write");
                        return;
                    }
                }
            }

            ZEJF_DEBUG(1, "avg diff: %.5fms, changed by %.2fms, goal: %.2fms, shift: %d, target shift: %d\n", avg_diff / 1000.0, change / 1000.0, goal / 1000.0, shift, shift_goal);
        }

        last_avg_diff = avg_diff;
        if (!calibrating) {
            if (last_avg_diff > statistics.highest_avg_diff) {
                statistics.highest_avg_diff = last_avg_diff;
            }
            if (last_avg_diff < statistics.lowest_avg_diff) {
                statistics.lowest_avg_diff = last_avg_diff;
            }
        }
        last_set = true;
    }
}

int64_t first_log_id = -1;
int first_log_num = 0;
int last_log_num = 0;

void next_sample(int shift, int log_num, int32_t value) {
    int64_t time = micros();
    if (first_log_id == -1) {
        first_log_id = time / (1000 * SAMPLE_TIME_MS) + 1;
        ZEJF_DEBUG(1, "calibrating %ld us\n", first_log_id * 1000 * SAMPLE_TIME_MS - time);
        printf("Calibrating, please wait...\n");
        first_log_num = log_num;
    } else {
        if (log_num == last_log_num) {
            return;
        } else if (log_num < last_log_num) { // log num overflow
            first_log_id = first_log_id + (last_log_num - first_log_num) + 1;
            first_log_num = log_num;
        } else if (log_num - last_log_num > 1) {
            statistics.arduino_gaps++;
            ZEJF_DEBUG(2, "ERR COMM GAP!\n");
        }

        int64_t expected_time = (first_log_id + (log_num - first_log_num)) * SAMPLE_TIME_MS * 1000;
        int64_t diff = time - expected_time;
        diff_control(diff, shift);
    }

    if (!calibrating) {
        pthread_mutex_lock(&log_queue_lock);
        next_log(value, first_log_id + (log_num - first_log_num));
        pthread_mutex_unlock(&log_queue_lock);

        sem_post(&log_queue_semaphore);
    }
    last_log_num = log_num;
}

bool decode(char *buffer) {
    if (buffer[0] != 's') {
        return false;
    }
    char *v = strchr(buffer, 'v');
    if (v == NULL) {
        return false;
    }
    char *l = strchr(buffer, 'l');
    if (l == NULL) {
        return false;
    }
    memset(v, '\0', 1);
    memset(l, '\0', 1);
    int shift = atoi(buffer + 1);
    int log_num = atoi(l + 1);
    int32_t value = atol(v + 1);

    next_sample(shift, log_num, value);

    return true;
}

#define BUFFER_SIZE 1024
#define LINE_BUFFER_SIZE 64
#define IGNORE 10

void run_reader(char *serial, int serial_port) {
    count_diffs = 0;
    sum_diffs = 0;
    first_log_id = -1;
    calibrating = true;
    last_set = false;
    last_log_id = -1;
    last_avg_diff = 0;
    char msg[2];
    msg[0] = 'r';
    msg[1] = '0' + options->sample_rate_id;
    ZEJF_DEBUG(0, "waiting for serial device\n");
    sleep(3);
    if (write(serial_port, msg, 2) == -1) {
        perror("write");
        goto end;
    }

    printf("Serial port connected!\n");

    char buffer[BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE];
    int line_buffer_ptr = 0;

    struct stat stats;

    while (true) {
        ssize_t count = read(serial_port, buffer, BUFFER_SIZE);
        
        // maybe EOF
        if (count <= 0) {
            if (stat(serial, &stats) == -1) {
                break;
            }
        }

        for (ssize_t i = 0; i < count; i++) {
            line_buffer[line_buffer_ptr] = buffer[i];
            line_buffer_ptr++;
            if (buffer[i] == '\n') {
                line_buffer[line_buffer_ptr - 1] = '\0';
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                if (!decode(line_buffer)) {
                    ZEJF_DEBUG(0, "Arduino: %s\n", line_buffer);
                }
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                line_buffer_ptr = 0;
            }
            if (line_buffer_ptr == LINE_BUFFER_SIZE - 1) {
                ZEJF_DEBUG(2, "ERR SERIAL READER BUFF OVERFLOW\n");
                line_buffer_ptr = 0;
            }
        }
    }

end:

    printf("Serial port closed!\n");
    ZEJF_DEBUG(0, "serial reader thread finish\n");
    serial_port_running = false;
    pthread_exit(0);
}

void *run_serial(void *arg) {
    char *serial = (char *) arg;
    serial_port_needs_join = true;
    serial_port_running = true;
    ZEJF_DEBUG(0, "serial reader thread start\n");

    printf("Trying to open serial port %s\n", serial);
    // Open the serial port. Change device path as needed (currently set to an
    // standard FTDI USB-UART cable type device)
    serial_port = open(serial, O_RDWR);

    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
    if (tcgetattr(serial_port, &tty) != 0) {
        printf("Unable to open serial port %s: %s\n", serial, strerror(errno));
        ZEJF_DEBUG(1, "error %i from tcgetattr: %s\n", errno, strerror(errno));
        serial_port_running = false;
        pthread_exit(0);
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in
                            // communication (most common)
    tty.c_cflag &= ~CSIZE;  // Clear all bits that set the data size
    tty.c_cflag |= CS8;     // 8 bits per byte (most common)
    tty.c_cflag &=
            ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |=
            CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                   // Disable echo
    tty.c_lflag &= ~ECHOE;                  // Disable erasure
    tty.c_lflag &= ~ECHONL;                 // Disable new-line echo
    tty.c_lflag &= ~ISIG;                   // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
            ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g.
                           // newline chars)
    tty.c_oflag &=
            ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT
    // PRESENT ON LINUX) tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars
    // (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 100; // Wait for up to 1s (10 deciseconds), returning as soon
                           // as any data is received.
    tty.c_cc[VMIN] = 0;

    // Set in/out baud rate to be 38400
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Unable to open serial port %s: %s\n", serial, strerror(errno));
        ZEJF_DEBUG(1, "error %i from tcsetattr: %s\n", errno, strerror(errno));
        serial_port_running = false;
        pthread_exit(0);
    }

    run_reader(serial, serial_port);
    pthread_exit(0);
}

void serial_reader_destroy(void) {
    pthread_mutex_destroy(&log_queue_lock);
    sem_destroy(&log_queue_semaphore);
    free(log_queue);
}