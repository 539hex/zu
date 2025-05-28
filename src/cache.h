#ifndef CACHE_H
#define CACHE_H

#include "ds.h"     // For DataItem
#include <stddef.h> // For size_t

// Extern declarations for global cache variables
// Definitions will be in zu_cache.c
extern DataItem *memory_cache;
extern size_t cache_size;
extern size_t cache_capacity;

// --- Cache Management Function Declarations ---
void add_or_update_in_memory_cache(const char *key, const char *value, unsigned int hit_count);
void remove_from_memory_cache(const char *key);
void rebuild_memory_cache(DataItem *full_data_list, size_t list_size);
void free_global_cache(void); // To free the cache at the end

#endif // ZU_CACHE_H
