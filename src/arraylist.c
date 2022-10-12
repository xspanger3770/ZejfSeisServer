#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arraylist.h"

ArrayList *list_create(size_t item_size)
{
    ArrayList *list = malloc(sizeof(ArrayList));
    if (list == NULL) {
        perror("malloc");
        return NULL;
    }

    void *data = malloc(INITIAL_LIST_SIZE * item_size);
    if (data == NULL) {
        perror("malloc");
        free(list);
        return NULL;
    }

    list->capacity = INITIAL_LIST_SIZE;
    list->data = data;
    list->item_count = 0;
    list->item_size = item_size;
    return list;
}

bool list_append(ArrayList *list, void *item)
{
    if (list == NULL) {
        return false;
    }

    if (list->item_count == list->capacity) {
        void *new_data = realloc(list->data, list->capacity * 2 * list->item_size);
        if (new_data == NULL) {
            perror("realloc");
            return false;
        }
        list->data = new_data;
        list->capacity *= 2;
    }

    list->item_count++;
    memcpy(list_get(list, list->item_count - 1), item, list->item_size);
    return true;
}

void list_destroy(ArrayList *list, void(destructor)(void **))
{
    if (list == NULL) {
        return;
    }
    if (destructor != NULL) {
        for (size_t i = 0; i < list->item_count; i++) {
            destructor(list_get(list, i));
        }
    }
    free(list->data);
    free(list);
}

void *list_get(ArrayList *list, size_t index)
{
    if (list == NULL || index >= list->item_count) {
        return NULL;
    }

    return list->data + list->item_size * index;
}

inline bool list_is_empty(ArrayList *list)
{
    return list->item_count == 0;
}

bool list_clear(ArrayList *list, void(destructor)(void **))
{
    if (list == NULL) {
        return false;
    }
    if (destructor != NULL) {
        for (size_t i = 0; i < list->item_count; i++) {
            destructor(list_get(list, i));
        }
    }
    void *new_data = realloc(list->data, INITIAL_LIST_SIZE * list->item_size);
    if (new_data == NULL) {
        perror("realloc");
        return false; // TODO can this even happen?
    }
    list->data = new_data;
    list->capacity = INITIAL_LIST_SIZE;
    list->item_count = 0;
    return true;
}

bool list_remove(ArrayList *list, size_t index, void(destructor)(void **))
{
    if (list == NULL || index >= list->item_count) {
        return false;
    }
    if (destructor != NULL) {
        destructor(list_get(list, index));
    }
    for (size_t i = index; i < list->item_count - 1; i++) {
        memcpy(list_get(list, i), list_get(list, i + 1), list->item_size);
    }

    list->item_count -= 1;

    if (list->item_count >= INITIAL_LIST_SIZE && list->item_count == list->capacity / 2) {
        void *new_data = realloc(list->data, list->item_count * list->item_size);
        if (new_data == NULL) {
            perror("realloc");
            return false; // TODO can this even happen?
        }
        list->data = new_data;
        list->capacity = list->item_count;
    }

    return true;
}

void *list_find(ArrayList *list, bool(comparator)(void **))
{
    for (size_t i = 0; i < list->item_count; i++) {
        void *item = list_get(list, i);
        if (comparator(item)) {
            return item;
        }
    }
    return NULL;
}