#include "cache.h"
#include "config.h"
#include "ds.h"
#include "io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Definition of global cache variables
DataItem *memory_cache = NULL;
size_t cache_size = 0;
size_t cache_capacity = 0;

// Definition of the global configuration variable
unsigned int HIT_THRESHOLD_FOR_CACHING = 2;

void add_or_update_in_memory_cache(const char *key, const char *value, unsigned int hit_count)
{
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key) == 0)
        {
            free(memory_cache[i].value);
            memory_cache[i].value = my_strdup(value);
            memory_cache[i].hit_count = hit_count;
            if (!memory_cache[i].value)
            { // my_strdup might fail
                perror("Failed to allocate memory for updated cache value");
                // It might be necessary to remove the item or handle the error
            }
            return;
        }
    }
    // Not found, add new
    ensure_list_capacity(&memory_cache, &cache_capacity, cache_size + 1);
    memory_cache[cache_size].key = my_strdup(key);
    memory_cache[cache_size].value = my_strdup(value);
    memory_cache[cache_size].hit_count = hit_count;
    if (!memory_cache[cache_size].key || !memory_cache[cache_size].value)
    {
        perror("Failed to allocate memory for new cache entry");
        free_data_item_contents(&memory_cache[cache_size]); // Clean partially allocated
        // Do not increment cache_size if allocation fails
        return;
    }
    cache_size++;
}

void remove_from_memory_cache(const char *key)
{
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key) == 0)
        {
            free_data_item_contents(&memory_cache[i]);
            // Move the last element to this position to fill the gap
            if (i < cache_size - 1)
            {
                memory_cache[i] = memory_cache[cache_size - 1];
                // Invalidate the last element to avoid double free if DataItem contained pointers not managed by free_data_item_contents
                memory_cache[cache_size - 1].key = NULL;
                memory_cache[cache_size - 1].value = NULL;
            }
            cache_size--;
            // Optional: resize buffer if cache_size is much smaller than cache_capacity
            return;
        }
    }
}

void rebuild_memory_cache(DataItem *full_data_list, size_t list_size)
{
    free_data_list(&memory_cache, &cache_size, &cache_capacity);
    for (size_t i = 0; i < list_size; i++)
    {
        if (full_data_list[i].hit_count >= HIT_THRESHOLD_FOR_CACHING)
        {
            add_or_update_in_memory_cache(full_data_list[i].key, full_data_list[i].value, full_data_list[i].hit_count);
        }
    }
}

void free_global_cache(void)
{
    free_data_list(&memory_cache, &cache_size, &cache_capacity);
}