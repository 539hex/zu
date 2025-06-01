#include "cache.h"
#include "config.h"
#include "ds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Definition of global cache variables
DataItem *memory_cache = NULL;
int cache_size = 0;  // Current number of items in cache

void add_or_update_in_memory_cache(const char *key, const char *value)
{
    if (!memory_cache) {
        // Initialize cache with CACHE_SIZE entries
        memory_cache = calloc(CACHE_SIZE, sizeof(DataItem));
        if (!memory_cache) {
            printf("Error: Failed to allocate memory cache\n");
            return;
        }
    }

    // First try to find and update existing key
    for (size_t i = 0; i < cache_size; i++)
    {
        if (memory_cache[i].key && strcmp(memory_cache[i].key, key) == 0)
        {
            free(memory_cache[i].value);
            memory_cache[i].value = my_strdup(value);
            if (memory_cache[i].value)
            {
                memory_cache[i].last_accessed = time(NULL); // Update last accessed time
                memory_cache[i].hit_count++; // Increment hit count
            }
            else
            {
                perror("Failed to allocate memory for updated cache value");
                free_data_item_contents(&memory_cache[i]);
            }
            return;
        }
    }

    // Not found, add new if we have space
    if (cache_size < CACHE_SIZE) {
        memory_cache[cache_size].key = my_strdup(key);
        memory_cache[cache_size].value = my_strdup(value);
        if (memory_cache[cache_size].key && memory_cache[cache_size].value) {
            memory_cache[cache_size].hit_count = 1;
            memory_cache[cache_size].last_accessed = time(NULL);
            cache_size++;
        } else {
            free_data_item_contents(&memory_cache[cache_size]);
            printf("Failed to add to cache: memory allocation failed\n");
        }
    } else {
        // Cache is full, implement LRU eviction
        int oldest_idx = 0;
        time_t oldest_time = memory_cache[0].last_accessed;
        
        // Find least recently used item
        for (int i = 1; i < cache_size; i++) {
            if (memory_cache[i].last_accessed < oldest_time) {
                oldest_time = memory_cache[i].last_accessed;
                oldest_idx = i;
            }
        }
        
        // Replace the oldest item
        free_data_item_contents(&memory_cache[oldest_idx]);
        memory_cache[oldest_idx].key = my_strdup(key);
        memory_cache[oldest_idx].value = my_strdup(value);
        if (memory_cache[oldest_idx].key && memory_cache[oldest_idx].value) {
            memory_cache[oldest_idx].hit_count = 1;
            memory_cache[oldest_idx].last_accessed = time(NULL);
        } else {
            free_data_item_contents(&memory_cache[oldest_idx]);
            printf("Failed to replace cache item: memory allocation failed\n");
        }
    }
}

void remove_from_memory_cache(const char *key)
{
    if (!memory_cache || !key) return;

    for (size_t i = 0; i < cache_size; i++)
    {
        if (memory_cache[i].key && strcmp(memory_cache[i].key, key) == 0)
        {
            free_data_item_contents(&memory_cache[i]);
            
            // If this wasn't the last item, move the last item here
            if (i < cache_size - 1) {
                memory_cache[i] = memory_cache[cache_size - 1];
                // Clear the last item to avoid double free
                memory_cache[cache_size - 1].key = NULL;
                memory_cache[cache_size - 1].value = NULL;
            }
            
            cache_size--;
            printf("Removed from cache (current size: %d/%d)\n", cache_size, CACHE_SIZE);
            return;
        }
    }
}

void free_global_cache(void)
{
    if (memory_cache) {
        // Free all items in the cache
        for (int i = 0; i < cache_size; i++) {
            free_data_item_contents(&memory_cache[i]);
        }
        free(memory_cache);
        memory_cache = NULL;
        cache_size = 0;
    }
}

