#include "cache.h"
#include "config.h"
#include "ds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Definition of global cache variable
HashTable *memory_cache = NULL;

void init_cache(void)
{
    if (memory_cache == NULL)
    {
        memory_cache = create_hash_table(CACHE_SIZE);
    }
}

void free_cache(void)
{
    if (memory_cache)
    {
        free_hash_table(memory_cache);
        memory_cache = NULL;
    }
}

void add_to_cache(const char *key, const char *value)
{
    if (memory_cache == NULL) init_cache();
    hash_table_insert(memory_cache, key, value);
}

DataItem *get_from_cache(const char *key)
{
    if (memory_cache == NULL) return NULL;
    DataItem* item = hash_table_search(memory_cache, key);
    if (item) {
        item->hit_count++;
        item->last_accessed = time(NULL);
    }
    return item;
}

void remove_from_cache(const char *key)
{
    if (memory_cache == NULL) return;
    hash_table_remove(memory_cache, key);
}

