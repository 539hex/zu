#include "cache.h"
#include "config.h"
#include "ds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Definition of global cache variable
HashTable *memory_cache = NULL;

// Definition of mutex variables
pthread_mutex_t cache_mutex;
pthread_mutex_t file_mutex;

void init_cache(void)
{
    if (memory_cache == NULL)
    {
        memory_cache = create_hash_table(CACHE_SIZE);

        // Initialize mutexes for thread safety
        pthread_mutex_init(&cache_mutex, NULL);
        pthread_mutex_init(&file_mutex, NULL);
    }
}

void free_cache(void)
{
    if (memory_cache)
    {
        free_hash_table(memory_cache);
        memory_cache = NULL;

        // Destroy mutexes
        pthread_mutex_destroy(&cache_mutex);
        pthread_mutex_destroy(&file_mutex);
    }
}

// Internal function that assumes mutex is already locked
static void remove_from_cache_internal(const char *key)
{
    if (memory_cache == NULL) return;
    hash_table_remove(memory_cache, key);
}

void add_to_cache(const char *key, const char *value)
{
    pthread_mutex_lock(&cache_mutex);
    if (memory_cache == NULL) init_cache();
    hash_table_insert(memory_cache, key, value);
    // Update last_accessed for the item
    DataItem *item = hash_table_search(memory_cache, key);
    if (item) item->last_accessed = (unsigned int)time(NULL);
    pthread_mutex_unlock(&cache_mutex);
}

DataItem *get_from_cache(const char *key)
{
    pthread_mutex_lock(&cache_mutex);
    if (memory_cache == NULL) {
        pthread_mutex_unlock(&cache_mutex);
        return NULL;
    }
    DataItem* item = hash_table_search(memory_cache, key);
    if (item) {
        if (time(NULL) - item->last_accessed > CACHE_TTL) {
            remove_from_cache_internal(key);
            pthread_mutex_unlock(&cache_mutex);
            return NULL;
        }
        item->hit_count++;
        item->last_accessed = (unsigned int)time(NULL);
    }
    pthread_mutex_unlock(&cache_mutex);
    return item;
}

void remove_from_cache(const char *key)
{
    pthread_mutex_lock(&cache_mutex);
    remove_from_cache_internal(key);
    pthread_mutex_unlock(&cache_mutex);
}

