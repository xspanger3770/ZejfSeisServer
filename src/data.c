#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "arraylist.h"
#include "data.h"
#include "my_string.h"
#include "time_utils.h"
#include "scheduler.h"

const int SAMPLE_RATES[5] = { 20, 40, 60, 100, 200 };
int SAMPLES_PER_SECOND;
int SAMPLE_TIME_MS;
int SAMPLES_IN_HOUR;

ArrayList *datahours;
pthread_mutex_t data_lock;
int64_t last_received_log_id = -1;
DataHour *current_datahour = NULL;
DataHour *last_datahour = NULL;

size_t datahour_get_size() {
    return sizeof(DataHour) + SAMPLES_IN_HOUR * sizeof(int32_t);
}

DataHour *datahour_create(int32_t hour_id) {
    DataHour *datahour = calloc(1, datahour_get_size());
    if (datahour == NULL) {
        perror("calloc");
        return NULL;
    }

    datahour->hour_id = hour_id;
    datahour->sample_count = 0;
    datahour->modified = false;
    datahour->last_access_ms = millis();
    for (int i = 0; i < SAMPLES_IN_HOUR; i++) {
        datahour->samples[i] = ERR_VAL;
    }
    return datahour;
}

void datahour_destroy(DataHour *datahour) {
    if (datahour == NULL) {
        return;
    }

    if (last_datahour != NULL && last_datahour->hour_id == datahour->hour_id) {
        last_datahour = NULL;
    }

    free(datahour);
}

int mkpath(char *file_path, mode_t mode) {
    for (char *p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

bool datahour_save(DataHour *dh) {
    if (dh == NULL) {
        return false;
    }

    String *file = get_datahour_path_newest(dh->hour_id);
    String *path = string_create(file->data);
    memset(strrchr(path->data, '/') + 1, '\0', 1);

    struct stat st = { 0 };

    if (stat(path->data, &st) == -1) {
        ZEJF_LOG(1, "Creating %s\n", path->data);
        if (mkpath(path->data, 0700) != 0) {
            perror("mkdir");
            string_destroy(file);
            string_destroy(path);
            return false;
        }
    }

    FILE *actual_file = fopen(file->data, "wb");
    if (actual_file == NULL) {
        perror("fopen");
        string_destroy(file);
        string_destroy(path);
        return false;
    }

    ZEJF_LOG(1, "Saving to %s, %d\n", file->data, dh->hour_id);

    bool result;
    if ((result = (fwrite(dh, datahour_get_size(), 1, actual_file) == 1))) {
        dh->modified = false;
    }

    fclose(actual_file);
    string_destroy(file);
    string_destroy(path);

    return result;
}

DataHour *datahour_load(FILE *file) {
    if (file == NULL) {
        return NULL;
    }

    DataHour *datahour = malloc(datahour_get_size());
    if (datahour == NULL) {
        perror("malloc");
        return NULL;
    }

    if (fread(datahour, datahour_get_size(), 1, file) != 1) {
        if (errno != 0) {
            perror("fread");
        } else {
            ZEJF_LOG(2, "Read failed\n");
        }
        free(datahour);
        return NULL;
    }

    datahour->modified = false;
    datahour->last_access_ms = millis();

    return datahour;
}

void datahour_destructor(void **ptr) {
    if (ptr == NULL) {
        return;
    }
    DataHour *dh = *((DataHour **) ptr);
    datahour_destroy(dh);
}

DataHour *get_datahour(int32_t hour_id, bool load_from_file, bool create_new) {
    // optimalisation
    if (last_datahour != NULL && last_datahour->hour_id == hour_id) {
        return last_datahour;
    }

    for (size_t i = 0; i < datahours->item_count; i++) {
        DataHour *dh = *(DataHour **) list_get(datahours, i);
        if (dh->hour_id == hour_id) {
            last_datahour = dh;
            return dh;
        }
    }

    DataHour *dh = NULL;

    bool modified = false;

    if (load_from_file) {
        String *path = get_datahour_path_newest(hour_id);
        if (path == NULL) {
            return NULL;
        }
        FILE *file = fopen(path->data, "rb");
        if (file != NULL) {
            ZEJF_LOG(1, "Load %s\n", path->data);
            dh = datahour_load(file);
            fclose(file);
        } else {
            perror(path->data);
        }
        string_destroy(path);
    }

    if (dh != NULL && dh->hour_id != hour_id) {
        ZEJF_LOG(2, "Fatal: Loaded DataHour id doesn't match! (wanted %d, got %d) [%d]\n", hour_id, dh->hour_id, create_new);
        datahour_destroy(dh);
        dh = NULL;
    }

    if (create_new && dh == NULL) {
        dh = datahour_create(hour_id);
        ZEJF_LOG(0, "+1 DH\n");
    }

    if (dh == NULL) {
        return NULL;
    }

    if (modified) {
        dh->modified = true;
    }

    list_append(datahours, &dh);
    last_datahour = dh;
    return dh;
}

char months[12][10] = { "January\0", "February\0", "March\0", "April\0", "May\0", "June\0", "July\0", "August\0", "September\0", "October\0", "November\0", "December\0" };

String *get_datahour_path_old(int32_t hour_id) {
    String *result = string_create(MAIN_FOLDER);
    if (result == NULL) {
        return NULL;
    }
    char text[128];
    time_t now = hour_id * 60 * 60;
    struct tm *t = localtime(&now);

    snprintf(text, sizeof(text), "%d_sps/", SAMPLES_PER_SECOND);
    string_append(result, text);

    strftime(text, sizeof(text) - 1, "%Y/", t);
    string_append(result, text);
    string_append(result, months[t->tm_mon]);

    strftime(text, sizeof(text) - 1, "/%d/%HH.dat", t);
    string_append(result, text);

    return result;
}

String *get_datahour_path_new(int32_t hour_id) {
    String *result = string_create(MAIN_FOLDER);
    if (result == NULL) {
        return NULL;
    }
    char text[128];
    time_t now = hour_id * 60 * 60;
    struct tm *t = localtime(&now);

    snprintf(text, sizeof(text), "%d_sps/", SAMPLES_PER_SECOND);
    string_append(result, text);

    strftime(text, sizeof(text) - 1, "%Y/", t);
    string_append(result, text);
    string_append(result, months[t->tm_mon]);

    strftime(text, sizeof(text) - 1, "/%d/%HH", t);
    string_append(result, text);

    snprintf(text, sizeof(text), "_%d.dat", hour_id);
    string_append(result, text);

    return result;
}

String *get_datahour_path_newest(int32_t hour_id) {
    String *result = string_create(MAIN_FOLDER);
    if (result == NULL) {
        return NULL;
    }
    char text[128];
    time_t now = hour_id * 60 * 60;
    struct tm *t = localtime(&now);

    snprintf(text, sizeof(text), "%d_sps/", SAMPLES_PER_SECOND);
    string_append(result, text);

    strftime(text, sizeof(text) - 1, "%Y/", t);
    string_append(result, text);
    string_append(result, months[t->tm_mon]);

    strftime(text, sizeof(text) - 1, "/%d/%HH", t);
    string_append(result, text);

    snprintf(text, sizeof(text), "_%d.cs4", hour_id);
    string_append(result, text);

    return result;
}

int32_t get_log(int64_t log_id) {
    int32_t hour_id = get_hour_id(log_id);
    DataHour *dh = get_datahour(hour_id, true, true);
    if (dh == NULL) {
        return ERR_VAL;
    }
    dh->last_access_ms = millis();
    return dh->samples[log_id % SAMPLES_IN_HOUR];
}

void log_data(int64_t log_id, int32_t val) {
    int32_t hour_id = get_hour_id(log_id);
    if (current_datahour == NULL || current_datahour->hour_id != hour_id) {
        current_datahour = get_datahour(hour_id, true, true);
        if (current_datahour == NULL) {
            return;
        }
    }
    current_datahour->modified = true;
    current_datahour->last_access_ms = millis();
    if (current_datahour->samples[log_id % SAMPLES_IN_HOUR] == ERR_VAL && val != ERR_VAL) {
        current_datahour->sample_count++;
    }
    current_datahour->samples[log_id % SAMPLES_IN_HOUR] = val;
    last_received_log_id = log_id;
}

void data_init(void) {
    datahours = list_create(sizeof(DataHour *));

    struct stat st = { 0 };

    if (stat(MAIN_FOLDER, &st) == -1) {
        ZEJF_LOG(1, "Creating main directory...\n");
        if (mkdir(MAIN_FOLDER, 0700) != 0) {
            perror("mkdir");
            return;
        }
    }

    pthread_mutex_init(&data_lock, NULL);
}

void autosave() {
    pthread_mutex_lock(&data_lock);
    size_t i = 0;
    size_t count = 0;
    while (i < datahours->item_count) {
        DataHour *dh = *(DataHour **) list_get(datahours, i);
        if (dh->modified) {
            datahour_save(dh);
            count++;
        }
        i++;
    }
    pthread_mutex_unlock(&data_lock);

    ZEJF_LOG(1, "Saved %ld datahours\n", count);
}

size_t datahours_count() {
    return datahours->item_count;
}

void cleanup() {
    pthread_mutex_lock(&data_lock);
    size_t i = 0;
    size_t count = 0;
    int64_t current_millis = millis();
    int32_t current_hour_id = current_millis / (60 * 60 * 1000);
    while (i < datahours->item_count) {
        DataHour *dh = *(DataHour **) list_get(datahours, i);
        if (current_millis - dh->last_access_ms > STORE_TIME_MINUTES * 60 * 1000 && current_hour_id - dh->hour_id >= PERMANENTLY_LOADED_HOURS) {
            list_remove(datahours, i, datahour_destructor);
            count++;
            i--;
        }
        i++;
    }

    pthread_mutex_unlock(&data_lock);

    ZEJF_LOG(0, "destroyed %ld DataHours, current count: %ld\n", count, datahours->item_count);
}

void data_destroy(void) {
    autosave();
    list_destroy(datahours, datahour_destructor);
}

void *run_data_manager() {
    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        autosave();
        cleanup();
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(30);
    }
}