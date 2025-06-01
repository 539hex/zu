#ifndef DS_H
#define DS_H

#include <stdlib.h> // For size_t

// --- Data Structures ---
typedef struct
{
    char *key;
    char *value;
    unsigned int hit_count;     // Hit count for caching
    unsigned int last_accessed; // Timestamp of last access
} DataItem;

// --- Helper Function Declarations ---
char *my_strdup(const char *s);
void free_data_item_contents(DataItem *item);
void free_data_list(DataItem **list, size_t *size, size_t *capacity);
void ensure_list_capacity(DataItem **list, size_t *capacity, size_t needed_size);

#endif // DS_H
