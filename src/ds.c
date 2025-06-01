#include "ds.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *my_strdup(const char *s)
{
    if (s == NULL)
        return NULL;
    size_t len = strlen(s) + 1;
    char *new_s = malloc(len);
    if (new_s == NULL)
    {
        perror("my_strdup: malloc failed");
        exit(EXIT_FAILURE);
    }
    memcpy(new_s, s, len);
    return new_s;
}

void free_data_item_contents(DataItem *item)
{
    if (item)
    {
        free(item->key);
        item->key = NULL;
        free(item->value);
        item->value = NULL;
    }
}

void ensure_list_capacity(DataItem **list, size_t *capacity, size_t needed_size)
{
    if (*capacity < needed_size)
    {
        size_t new_capacity = (*capacity == 0) ? INITIAL_CAPACITY : *capacity * 2;
        if (new_capacity < needed_size)
        {
            new_capacity = needed_size;
        }
        DataItem *new_data_list = realloc(*list, new_capacity * sizeof(DataItem));
        if (new_data_list == NULL)
        {
            perror("Failed to reallocate data list memory");
            // Consider less drastic error handling if possible
            exit(EXIT_FAILURE);
        }
        *list = new_data_list;
        *capacity = new_capacity;
    }
}