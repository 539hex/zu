#include "ds.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h> // For UINT_MAX

// --- Hash Table Implementation ---

// A simple hash function (djb2)
unsigned int hash_function(const char *key, unsigned int size)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % size;
}

HashTable *create_hash_table(unsigned int size)
{
    HashTable *ht = malloc(sizeof(HashTable));
    if (!ht)
        return NULL;
    ht->size = size;
    ht->table = calloc(size, sizeof(DataItem *));
    if (!ht->table)
    {
        free(ht);
        return NULL;
    }
    return ht;
}

void free_hash_table(HashTable *ht)
{
    if (!ht)
        return;
    for (unsigned int i = 0; i < ht->size; i++)
    {
        DataItem *current = ht->table[i];
        while (current)
        {
            DataItem *next = current->next;
            free_data_item_contents(current);
            free(current);
            current = next;
        }
    }
    free(ht->table);
    free(ht);
}

void hash_table_insert(HashTable *ht, const char *key, const char *value)
{
    unsigned int index = hash_function(key, ht->size);
    DataItem *current = ht->table[index];
    DataItem *prev = NULL;

    // Check if key already exists
    while (current)
    {
        if (strcmp(current->key, key) == 0)
        {
            // Key found, update value
            free(current->value);
            current->value = my_strdup(value);
            return;
        }
        prev = current;
        current = current->next;
    }

    // Key not found, create new item
    DataItem *new_item = malloc(sizeof(DataItem));
    if (!new_item)
        return; // Handle allocation failure
    new_item->key = my_strdup(key);
    new_item->value = my_strdup(value);
    new_item->hit_count = 0;
    new_item->last_accessed = 0; // Or set current time
    new_item->next = NULL;

    if (prev)
    {
        prev->next = new_item;
    }
    else
    {
        ht->table[index] = new_item;
    }

    // After insert, check size and perform LRU eviction if needed
    static unsigned int cached_item_count = 0; // Cache the count to avoid full scans
    unsigned int current_items = 0;

    // Count items efficiently (with periodic full recount for accuracy)
    if (cached_item_count == 0 || (cached_item_count % 100) == 0) {
        // Periodic full recount to maintain accuracy
        current_items = 0;
        for (unsigned int i = 0; i < ht->size; i++) {
            DataItem *item = ht->table[i];
            while (item) {
                current_items++;
                item = item->next;
            }
        }
        cached_item_count = current_items;
    } else {
        current_items = cached_item_count + 1; // We just added one item
    }

    // Perform LRU eviction with safety bounds
    unsigned int eviction_attempts = 0;
    const unsigned int max_evictions = CACHE_SIZE * 2; // Safety limit

    while (current_items > CACHE_SIZE && eviction_attempts < max_evictions) {
        // Find LRU (lowest last_accessed) - avoid infinite loops
        DataItem *lru = NULL;
        unsigned int lru_time = UINT_MAX;

        // Scan hash table to find LRU item
        for (unsigned int i = 0; i < ht->size; i++) {
            DataItem *item = ht->table[i];
            while (item) {
                if (item->last_accessed < lru_time) {
                    lru_time = item->last_accessed;
                    lru = item;
                } else if (item->last_accessed == lru_time && item < lru) {
                    // Tie-breaker: prefer lower memory address to avoid loops
                    lru = item;
                }
                item = item->next;
            }
        }

        if (lru && lru->key) {
            // Attempt to remove the LRU item
            hash_table_remove(ht, lru->key);
            current_items--;
            cached_item_count = current_items;
            eviction_attempts++;
        } else {
            // No removable items found, break to prevent infinite loop
            break;
        }
    }

    // If we hit the safety limit, something is wrong - reset the cache
    if (eviction_attempts >= max_evictions) {
        // Emergency: clear the entire cache to prevent infinite loops
        for (unsigned int i = 0; i < ht->size; i++) {
            DataItem *item = ht->table[i];
            while (item) {
                DataItem *next = item->next;
                free_data_item_contents(item);
                free(item);
                item = next;
            }
            ht->table[i] = NULL;
        }
        cached_item_count = 0;
    }
}

DataItem *hash_table_search(HashTable *ht, const char *key)
{
    unsigned int index = hash_function(key, ht->size);
    DataItem *current = ht->table[index];
    while (current)
    {
        if (strcmp(current->key, key) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void hash_table_remove(HashTable *ht, const char *key)
{
    unsigned int index = hash_function(key, ht->size);
    DataItem *current = ht->table[index];
    DataItem *prev = NULL;

    while (current)
    {
        if (strcmp(current->key, key) == 0)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                ht->table[index] = current->next;
            }
            free_data_item_contents(current);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

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