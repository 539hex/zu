#include "commands.h"
#include "config.h"
#include "ds.h"
#include "cache.h"
#include "io.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// --- zset command ---
void zset_command(const char *key_to_set, const char *value_to_set)
{
    char *old_value = NULL;
    int result = find_key_on_disk(key_to_set, &old_value);

    if (result < 0)
    {
        printf("Error: Could not access data for zset.\n");
        return;
    }

    if (result == 0)
    {
        // Key doesn't exist, append new key-value pair
        if (append_key_to_disk(key_to_set, value_to_set) <= 0)
        {
            printf("Error: Failed to set new key.\n");
            return;
        }
        printf("OK\n");
        return;
    }

    // Key exists, update its value
    if (old_value) {
        free(old_value);  // Free the old value we got from find_key_on_disk
    }

    // Remove the old key-value pair
    if (remove_key_from_disk(key_to_set) < 0) {
        printf("Error: Could not remove old key-value pair.\n");
        return;
    }

    // Add the new key-value pair
    if (append_key_to_disk(key_to_set, value_to_set) <= 0) {
        printf("Error: Failed to update key.\n");
        return;
    }

    printf("OK\n");
}

// --- zget command ---
void zget_command(const char *key_to_get)
{
    // Search in cache first
    if (memory_cache == NULL) {
        memory_cache = calloc(CACHE_SIZE, sizeof(DataItem));
        if (!memory_cache) {
            printf("Error: Failed to allocate memory cache\n");
            return;
        }
    }

    for (int i = 0; i < cache_size; i++)
    {
        if (memory_cache[i].key && strcmp(memory_cache[i].key, key_to_get) == 0)
        {
            memory_cache[i].hit_count++;                // Increment hit count
            memory_cache[i].last_accessed = time(NULL); // Update last accessed time
            printf("%s\n", memory_cache[i].value);
            return;
        }
    }

    // Not in cache, search on disk
    char *value = NULL;
    int result = find_key_on_disk(key_to_get, &value);

    if (result < 0)
    {
        printf("Error: Could not access data.\n");
        return;
    }
    else if (result == 0)
    {
        printf("Key '%s' not found.\n", key_to_get);
        return;
    }

    // Make a copy of the value before adding to cache
    char *value_copy = my_strdup(value);
    if (!value_copy) {
        printf("Error: Failed to allocate memory for value copy.\n");
        free(value);
        return;
    }

    // Add to cache using the copy
    add_or_update_in_memory_cache(key_to_get, value_copy);
    
    // Print the value
    printf("%s\n", value_copy);
    
    // Now we can safely free the original value
    free(value);
    // Note: value_copy is now owned by the cache and will be freed when the cache is cleared
}

// --- zrm command ---
void zrm_command(const char *key_to_remove)
{
    // Remove from cache if present
    remove_from_memory_cache(key_to_remove);

    // Remove from disk
    int result = remove_key_from_disk(key_to_remove);

    if (result < 0)
    {
        printf("Error: Could not remove key (file error).\n");
    }
    else if (result == 0)
    {
        printf("Key '%s' not found.", key_to_remove);
    }
    else
    {
        printf("OK: Key '%s' removed.", key_to_remove);
    }
}

// --- zall command ---
void zall_command()
{
    if (print_all_data_from_disk() == 0)
    {
        printf("(empty or file error)\n");
    }
}

void cleanup_db_command(void)
{
    printf("Cleaning up database to remove duplicate keys...\n");
    int result = cleanup_duplicate_keys();
    if (result < 0)
    {
        printf("Error: Could not clean up database (file error).\n");
    }
    else if (result == 0)
    {
        printf("Database is empty or does not exist.\n");
    }
    else
    {
        printf("Database cleanup complete.\n");
    }
}

void init_db_command()
{
    // Initialize random number generator with current time
    srand(time(NULL));

    // Define min and max lengths for keys and values
    const int MIN_LENGTH = 4;   // Minimum length for readability
    const int MAX_LENGTH = 64;  // Maximum length to keep things reasonable

    printf("Initializing database with %d random key-value pairs...\n", INIT_DB_SIZE);

    // Open file in write mode to start fresh
    FILE *file = fopen(FILENAME, "wb");
    if (!file) {
        printf("Error: Could not open database file for writing.\n");
        return;
    }


    for (int i = 0; i < INIT_DB_SIZE; i++)
    {
        // Generate random lengths for this pair
        int key_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        int value_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));

        // Allocate buffers with the random lengths
        char *buffer_key = malloc(key_length + 1);   // +1 for null terminator
        char *buffer_value = malloc(value_length + 1); // +1 for null terminator

        if (!buffer_key || !buffer_value) {
            printf("Error: Failed to allocate memory for key-value pair.\n");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return;
        }

        // Generate random strings of the chosen lengths
        generate_random_alphanumeric(buffer_key, key_length);
        generate_random_alphanumeric(buffer_value, value_length);

        // Write directly to file
        if (!write_item_to_file(file, buffer_key, buffer_value)) {
            printf("Error: Failed to write key-value pair to file.\n");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return;
        }

        printf("Inserted item %d / %d (key length: %d, value length: %d)\n", 
               i + 1, INIT_DB_SIZE, key_length, value_length);

        // Free the temporary buffers
        free(buffer_key);
        free(buffer_value);
    }

    fclose(file);
    printf("Database initialization complete.\n");
}

void cache_status(void)
{
    if (!memory_cache) {
        printf("Cache is not initialized\n");
        return;
    }
    
    printf("Cache status: %d/%d items used\n", cache_size, CACHE_SIZE);
    for (int i = 0; i < cache_size; i++) {
        if (memory_cache[i].key) {
            printf("  [%d] Key: %s, Value: %s, Hits: %u, Last accessed: %u\n",
                   i, memory_cache[i].key, memory_cache[i].value,
                   memory_cache[i].hit_count, memory_cache[i].last_accessed);
        }
    }
}