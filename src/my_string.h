#ifndef MY_STRING_H
#define MY_STRING_H

#include <stddef.h>

typedef struct my_string_t
{
    char *data;
    size_t size;
} String;

String *string_empty();

String *string_create(char *data);

void string_destroy(String *str);

int string_append(String *str, char *data);

#endif