#ifndef CACHE_H
#define CACHE_H

#include "ds.h"     // For DataItem, HashTable
#include <stddef.h> // For size_t
#include <pthread.h>

// Extern declarations for global cache variables
extern HashTable *memory_cache;

// Mutex for protecting cache operations
extern pthread_mutex_t cache_mutex;

// Mutex for protecting file operations (thread-safe file access)
extern pthread_mutex_t file_mutex;

// --- Cache Management Function Declarations ---
void init_cache(void);
void free_cache(void);
void add_to_cache(const char *key, const char *value);
DataItem *get_from_cache(const char *key);
void remove_from_cache(const char *key);

#endif // CACHE_H
