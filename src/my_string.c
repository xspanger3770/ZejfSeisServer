#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_string.h"

String *string_empty()
{
    String *str = malloc(sizeof(String));
    if (str == NULL) {
        perror("malloc");
        return NULL;
    }
    str->data = malloc(sizeof(char));
    if (str->data == NULL) {
        perror("malloc");
        free(str);
        return NULL;
    }
    str->data[0] = '\0';
    str->size = 1;
    return str;
}

String *string_create(char *data)
{
    String *str = malloc(sizeof(String));
    if (str == NULL) {
        perror("malloc");
        return NULL;
    }
    str->size = strlen(data) + 1;
    str->data = malloc(str->size * sizeof(char));
    if (str->data == NULL) {
        perror("malloc");
        free(str);
        return NULL;
    }
    strcpy(str->data, data);
    return str;
}

int string_append(String *str, char *data)
{
    size_t previous_size = str->size;
    str->size += strlen(data);
    char *new_data = realloc(str->data, str->size * sizeof(char));
    if (new_data == NULL) {
        return -1;
    }
    str->data = new_data;
    strcpy(str->data + previous_size - 1, data);
    return 0;
}

void string_destroy(String *str)
{
    if (str == NULL) {
        return;
    }
    free(str->data);
    free(str);
}
