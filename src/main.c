#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "data.h"
#include "scheduler.h"
#include "serial_reader.h"

void print_usage(void)
{
    printf("Usage: -s <serial port> -i <ip address> -p <port number> -r <sample rate>\n");
}

void print_sample_rate_usage(){
    printf("Sample rate is selected like this:\n");
    for(int i = 0; i < 5; i++){
        printf("%d: %dHz\n", i, SAMPLE_RATES[i]);
    }
}

int main(int argc, char *argv[])
{
    char *serial = "/dev/ttyUSB0";
    char *ip = "0.0.0.0";
    int port = 6222;
    int sample_rate = 40;
    static struct option long_options[] = {
        { "serial", required_argument, 0, 's' },
        { "ip", required_argument, 0, 'i' },
        { "port", required_argument, 0, 'p' },
        { "sample_rate", required_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    int opt = 0;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "s:i:p:r:", long_options, &long_index)) != -1) {
        switch (opt) {
        case 's':
            serial = optarg;
            break;
        case 'i':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'r':
            sample_rate = atoi(optarg);
            break;
        default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    int sample_rate_id = -1;
    for (int i = 0; i < 5; i++) {
        if (SAMPLE_RATES[i] == sample_rate) {
            sample_rate_id = i;
        }
    }

    if (sample_rate_id == -1) {
        print_sample_rate_usage();
        exit(1);
    }

    SAMPLES_PER_SECOND = sample_rate;
    SAMPLE_TIME_MS = 1000 / SAMPLES_PER_SECOND;
    SAMPLES_IN_HOUR = SAMPLES_PER_SECOND * 60 * 60;

    printf("Starting ZejfCSeis with serial %s, ip %s:%d\n", serial, ip, port);

    String *ip_string = string_create(ip);
    String *serial_string = string_create(serial);

    Options options = {
        .ip_address = ip_string,
        .port = port,
        .serial = serial_string,
        .sample_rate_id = sample_rate_id
    };

    //test2();
    run_threads(&options);

    string_destroy(ip_string);
    string_destroy(serial_string);

    return EXIT_SUCCESS;
}
