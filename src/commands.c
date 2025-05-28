#include "commands.h"
#include "config.h"
#include "ds.h"
#include "cache.h"
#include "io.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// --- zset command ---
void zset_command(const char *key_to_set, const char *value_to_set)
{

    char *old_value = NULL;
    unsigned int old_hit_count;
    int result = find_key_on_disk(key_to_set, &old_value, &old_hit_count);

    if (result < 0)
    {
        printf("Error: Could not access data for zset.\n");
        return;
    }

    if (result == 0)
    {
        // Key doesn't exist, append new key-value pair
        if (append_key_to_disk(key_to_set, value_to_set, 0) <= 0)
        {
            printf("Error: Failed to set new key.\n");
            return;
        }
        printf("OK\n");
        return;
    }
}

// --- zget command ---
void zget_command(const char *key_to_get)
{
    // Search in cache first
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key_to_get) == 0)
        {
            memory_cache[i].hit_count++;
            const char *status = (memory_cache[i].hit_count >= HIT_THRESHOLD_FOR_CACHING) ? "HOT" : "COLD";
            printf("%s\n[%s, hits: %u]", memory_cache[i].value, status, memory_cache[i].hit_count);
            return;
        }
    }

    // Not in cache, search on disk
    char *value = NULL;
    unsigned int hit_count;
    int result = find_key_on_disk(key_to_get, &value, &hit_count);

    if (result < 0)
    {
        printf("Key '%s' not found (file error during search).\n", key_to_get);
        return;
    }
    else if (result == 0)
    {
        printf("Key '%s' not found.", key_to_get);
        return;
    }

    // Always show COLD status when reading from disk
    printf("%s \n[COLD, hits: %u]", value, hit_count);

    // If it reaches threshold, add to cache for next time
    if (hit_count >= HIT_THRESHOLD_FOR_CACHING)
    {
        add_or_update_in_memory_cache(key_to_get, value, hit_count);
    }

    free(value);
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

void init_db_command() {
    #define KEY_VALUE_LENGTH 32

    char buffer_key[KEY_VALUE_LENGTH+ 1];   // +1 for the null terminator
    char buffer_value[KEY_VALUE_LENGTH + 1]; // +1 for the null terminator

    printf("Initializing database with %d alphanumeric key-value pairs (length %d)...\n", INIT_DB_SIZE, KEY_VALUE_LENGTH);

    for (int i = 0; i < INIT_DB_SIZE; i++) {

        generate_random_alphanumeric(buffer_key, KEY_VALUE_LENGTH);
        generate_random_alphanumeric(buffer_value, KEY_VALUE_LENGTH);

        if (append_key_to_disk(buffer_key, buffer_value, 0) <= 0) {
            fprintf(stderr, "Error: Failed to set new key-value pair for item %d.\n", i + 1);
            return;
        }
        printf("Inserted item %d / %d \n", i + 1, INIT_DB_SIZE);
    }
    printf("Database initialization complete.\n");
}