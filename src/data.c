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

const int SAMPLE_RATES[5] = {20, 40, 60, 100, 200};
int SAMPLES_PER_SECOND;
int SAMPLE_TIME_MS;
int SAMPLES_IN_HOUR;

ArrayList *datahours;
pthread_mutex_t data_lock;
int64_t last_received_log_id = -1;
DataHour* current_datahour = NULL;
DataHour* last_datahour = NULL;

size_t datahour_get_size() {
    return sizeof(DataHour) + SAMPLES_IN_HOUR * sizeof(int32_t);
}

DataHour *datahour_create(int32_t hour_id)
{
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

void datahour_destroy(DataHour *datahour)
{
    if (datahour == NULL) {
        return;
    }

    if(last_datahour != NULL && last_datahour->hour_id == datahour->hour_id){
        last_datahour = NULL;
    }

    free(datahour);
}

bool datahour_save(DataHour *datahour, FILE *file)
{
    if (datahour == NULL || file == NULL) {
        return false;
    }

    return fwrite(datahour, datahour_get_size(), 1, file) == 1;
}

DataHour *datahour_load(FILE *file)
{
    if (file == NULL) {
        return NULL;
    }

    DataHour *datahour = malloc(datahour_get_size());
    if (datahour == NULL) {
        perror("malloc");
        return NULL;
    }

    if (fread(datahour, datahour_get_size(), 1, file) != 1) {
        if(errno != 0){
            perror("fread");
        } else {
            printf("Read failed\n");
        }
        free(datahour);
        return NULL;
    }

    datahour->modified = false;
    datahour->last_access_ms = millis();

    return datahour;
}

void datahour_destructor(void **ptr)
{
    if (ptr == NULL) {
        return;
    }
    DataHour *dh = *((DataHour **) ptr);
    datahour_destroy(dh);
}

DataHour *get_datahour(int32_t hour_id, bool load_from_file, bool create_new)
{
    // optimalisation
    if(last_datahour != NULL && last_datahour->hour_id == hour_id){
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
    
    if(load_from_file){
        String* path = get_datahour_path(hour_id);
        if(path == NULL){
            return NULL;
        }
        FILE* file = fopen(path->data, "rb");
        if(file != NULL){
            printf("load %s\n", path->data);
            dh = datahour_load(file);
            fclose(file);
        }else{
            perror(path->data);
        }
        string_destroy(path);
    }
    if (create_new && dh == NULL) {
        dh = datahour_create(hour_id);
        printf("+1 DH\n");
    }
    if(dh == NULL) {
        return NULL;
    }
    //printf("added %d %d, %p\n", hour_id, dh->hour_id, dh);
    list_append(datahours, &dh);
    last_datahour = dh;
    return dh;
}

char months[12][10] = { "January\0", "February\0", "March\0", "April\0", "May\0", "June\0", "July\0", "August\0", "September\0", "October\0", "November\0", "December\0" };

String *get_datahour_path(int32_t hour_id)
{
    String *result = string_create(MAIN_FOLDER);
    if(result == NULL){
        return NULL;
    }
    char text[128];
    time_t now = hour_id * 60 * 60;
    struct tm *t = localtime(&now);

    snprintf(text, sizeof(text), "%d_sps/", SAMPLES_PER_SECOND);
    string_append(result, text);
    
    strftime(text, sizeof(text) - 1, "%Y/", t); //
    string_append(result, text);
    string_append(result, months[t->tm_mon]);

    strftime(text, sizeof(text) - 1, "/%d/%HH.dat", t);
    string_append(result, text);

    return result;
}

int32_t get_log(int64_t log_id)
{
    int32_t hour_id = get_hour_id(log_id);
    DataHour *dh = get_datahour(hour_id, true, true);
    if (dh == NULL) {
        return ERR_VAL;
    }
    dh->last_access_ms = millis();
    return dh->samples[log_id % SAMPLES_IN_HOUR];
}

void log_data(int64_t log_id, int32_t val)
{
    int32_t hour_id = get_hour_id(log_id);
    if(current_datahour == NULL || current_datahour->hour_id != hour_id){
        current_datahour = get_datahour(hour_id, false, true);
        if (current_datahour == NULL) {
            printf("ERR: NULL\n");
            return;
        }
    }
    current_datahour->modified = true;
    current_datahour->last_access_ms = millis();
    if(current_datahour->samples[log_id % SAMPLES_IN_HOUR] == ERR_VAL && val != ERR_VAL){
        current_datahour->sample_count++;
    }
    current_datahour->samples[log_id % SAMPLES_IN_HOUR] = val;
    last_received_log_id = log_id;
}

bool datahour_exists(int32_t hour_id)
{
    String *path = get_datahour_path(hour_id);
    struct stat st = { 0 };

    bool result = stat(path->data, &st) != -1;
    string_destroy(path);
    return result;
}

void data_init(void)
{
    datahours = list_create(sizeof(DataHour *));

    struct stat st = { 0 };

    if (stat(MAIN_FOLDER, &st) == -1) {
        printf("creating main directory...\n");
        if (mkdir(MAIN_FOLDER, 0700) != 0) {
            perror("mkdir");
            return;
        }
    }

    int32_t hour_id_current = millis() / (1000 * 60 * 60);
    for (int i = 0; i < PERMANENTLY_LOADED_HOURS; i++) {
        String *path = get_datahour_path(hour_id_current);
        printf("checking dataHour: %s, %d\n", path->data, datahour_exists(hour_id_current));

        DataHour *dh = NULL;
        FILE *file = fopen(path->data, "rb");
        if (file == NULL) {
            perror("fopen");
            dh = datahour_create(hour_id_current);
            printf("created blank DataHour %d\n", dh->hour_id);
        } else {
            dh = datahour_load(file);
            fclose(file);
        }

        if (dh == NULL) {
            dh = datahour_create(hour_id_current);
        }

        list_append(datahours, &dh);

        string_destroy(path);
        hour_id_current--;
    }

    pthread_mutex_init(&data_lock, NULL);
}

int mkpath(char *file_path, mode_t mode)
{
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

void autosave()
{
    pthread_mutex_lock(&data_lock);
    size_t i = 0;
    size_t count = 0;
    while (i < datahours->item_count) {
        DataHour *dh = *(DataHour **) list_get(datahours, i);
        if (dh->modified) {
            String *file = get_datahour_path(dh->hour_id);
            String *path = string_create(file->data);
            memset(strrchr(path->data, '/') + 1, '\0', 1);

            struct stat st = { 0 };

            if (stat(path->data, &st) == -1) {
                printf("creating %s\n", path->data);
                if (mkpath(path->data, 0700) != 0) {
                    perror("mkdir");
                    string_destroy(file);
                    string_destroy(path);
                    return;
                }
            }

            FILE *actual_file = fopen(file->data, "wb");
            if (actual_file == NULL) {
                perror("fopen");
                string_destroy(file);
                string_destroy(path);
                return;
            }

            printf("saving to %s, %d\n", file->data, dh->hour_id);
            datahour_save(dh, actual_file);

            count++;
            dh->modified = false;
            fclose(actual_file);
            string_destroy(file);
            string_destroy(path);
        }
        i++;
    }
    pthread_mutex_unlock(&data_lock);

    printf("saved %ld datahours\n", count);
}

size_t datahours_count()
{
    return datahours->item_count;
}

void cleanup()
{
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

    printf("destroyed %ld DataHours, current count: %ld\n", count, datahours->item_count);
}

void data_destroy(void)
{
    autosave();
    list_destroy(datahours, datahour_destructor);
}

void *run_data_manager()
{
    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        autosave();
        cleanup();
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(30);
    }
}