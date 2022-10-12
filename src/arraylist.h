#ifndef ARRAYLIST_H
#define ARRAYLIST_H

#include <stdbool.h>
#include <stddef.h>

#define INITIAL_LIST_SIZE 10

typedef struct arraylist_t
{
    char *data;
    size_t capacity;
    size_t item_size;
    size_t item_count;
} ArrayList;

ArrayList *list_create(size_t item_size);

void list_destroy(ArrayList *list, void(destructor)(void **));

void *list_get(ArrayList *list, size_t index);

bool list_append(ArrayList *list, void *item);

bool list_remove(ArrayList *list, size_t index, void(destructor)(void **));

bool list_clear(ArrayList *list, void(destructor)(void **));

void *list_find(ArrayList *list, bool(comparator)(void **));

bool list_is_empty(ArrayList *list);

#endif