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

/**
 * @brief Sets or updates a key-value pair in the database
 *
 * This function first checks if the database exists, then either:
 * - Appends a new key-value pair if the key doesn't exist
 * - Updates the value if the key already exists
 *
 * @param key_to_set The key to set or update
 * @param value_to_set The value to associate with the key
 * @return int Returns 1 on success, -1 on failure
 */
int zset_command(const char *key_to_set, const char *value_to_set)
{
    // First ensure database exists
    if (!ensure_database_exists())
    {
        return -1; // Database doesn't exist and user chose not to create it
    }

    char *old_value = NULL;
    int result = find_key_on_disk(key_to_set, &old_value);

    if (result < 0)
    {
        printf("Error: Could not access data for zset.\n");
        return -1;
    }

    if (result == 0)
    {
        // Key doesn't exist, append new key-value pair
        if (append_key_to_disk(key_to_set, value_to_set) <= 0)
        {
            printf("Error: Failed to set new key.\n");
            return -1;
        }
        printf("OK\n");
        return 1;
    }

    // Key exists, update its value
    if (old_value)
    {
        free(old_value); // Free the old value we got from find_key_on_disk
    }

    // Remove the old key-value pair
    if (remove_key_from_disk(key_to_set) < 0)
    {
        printf("Error: Could not remove old key-value pair.\n");
        return -1;
    }

    // Add the new key-value pair
    if (append_key_to_disk(key_to_set, value_to_set) <= 0)
    {
        printf("Error: Failed to update key.\n");
        return -1;
    }

    printf("OK\n");
    return 1;
}

/**
 * @brief Retrieves the value associated with a key
 *
 * This function implements a two-level lookup:
 * 1. First checks the in-memory cache
 * 2. If not found in cache, searches the disk database
 * If found on disk, the key-value pair is added to the cache
 *
 * @param key_to_get The key to look up
 * @return char* Returns a pointer to the value string if found, NULL otherwise.
 *               The returned string is owned by the cache and should not be freed by the caller
 */
char *zget_command(const char *key_to_get)
{
    // Search in cache first
    if (memory_cache == NULL)
    {
        memory_cache = calloc(CACHE_SIZE, sizeof(DataItem));
        if (!memory_cache)
        {
            printf("Error: Failed to allocate memory cache\n");
            return NULL;
        }
    }

    for (int i = 0; i < cache_size; i++)
    {
        if (memory_cache[i].key && strcmp(memory_cache[i].key, key_to_get) == 0)
        {
            memory_cache[i].hit_count++;                // Increment hit count
            memory_cache[i].last_accessed = time(NULL); // Update last accessed time
            printf("%s\n", memory_cache[i].value);
            return memory_cache[i].value;
        }
    }

    // Not in cache, search on disk
    char *value = NULL;
    int result = find_key_on_disk(key_to_get, &value);

    if (result < 0)
    {
        printf("Error: Could not access data.\n");
        return NULL;
    }
    else if (result == 0)
    {
        printf("Key '%s' not found.\n", key_to_get);
        return NULL;
    }

    // Make a copy of the value before adding to cache
    char *value_copy = my_strdup(value);
    if (!value_copy)
    {
        printf("Error: Failed to allocate memory for value copy.\n");
        free(value);
        return NULL;
    }

    // Add to cache using the copy
    add_or_update_in_memory_cache(key_to_get, value_copy);

    // Print the value
    printf("%s\n", value_copy);

    // Now we can safely free the original value
    free(value);
    // Note: value_copy is now owned by the cache and will be freed when the cache is cleared
    return value_copy;
}

/**
 * @brief Removes a key-value pair from both cache and disk
 *
 * This function removes the specified key-value pair from:
 * 1. The in-memory cache (if present)
 * 2. The disk database
 *
 * @param key_to_remove The key to remove
 * @return int Returns 1 if key was found and removed, 0 if key not found, -1 on error
 */
int zrm_command(const char *key_to_remove)
{
    // Remove from cache if present
    remove_from_memory_cache(key_to_remove);

    // Remove from disk
    int result = remove_key_from_disk(key_to_remove);

    if (result < 0)
    {
        printf("Error: Could not remove key (file error).\n");
        return -1;
    }
    else if (result == 0)
    {
        printf("Key '%s' not found.\n", key_to_remove);
        return 0;
    }
    else
    {
        printf("OK: Key '%s' removed.\n", key_to_remove);
        return 1;
    }
}

/**
 * @brief Lists all key-value pairs in the database
 *
 * This function reads and prints all key-value pairs stored in the disk database.
 * The output format is one key-value pair per line.
 *
 * @return int Returns 1 if successful, -1 if database is empty or on error
 */
int zall_command()
{
    int result = print_all_data_from_disk();
    if (result <= 0) // Changed from == 0 to <= 0 to handle both empty and error cases
    {
        printf("(empty or file error)\n");
        return -1;
    }
    return 1;
}

/**
 * @brief Initializes the database with random key-value pairs
 *
 * This function creates a new database file and populates it with
 * INIT_DB_SIZE random key-value pairs. Each key and value is a
 * random alphanumeric string with length between MIN_LENGTH and MAX_LENGTH.
 *
 * @return int Returns 1 on successful initialization, -1 on error
 */
int init_db_command()
{
    // Initialize random number generator with current time
    srand(time(NULL));

    // Define min and max lengths for keys and values
    const int MIN_LENGTH = 4;  // Minimum length for readability
    const int MAX_LENGTH = 64; // Maximum length to keep things reasonable

    printf("Initializing database with %d random key-value pairs...\n", INIT_DB_SIZE);

    // Open file in write mode to start fresh
    FILE *file = fopen(FILENAME, "wb");
    if (!file)
    {
        printf("Error: Could not open database file for writing.\n");
        return -1;
    }

    for (int i = 0; i < INIT_DB_SIZE; i++)
    {
        // Generate random lengths for this pair
        int key_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        int value_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));

        // Allocate buffers with the random lengths
        char *buffer_key = malloc(key_length + 1);     // +1 for null terminator
        char *buffer_value = malloc(value_length + 1); // +1 for null terminator

        if (!buffer_key || !buffer_value)
        {
            printf("Error: Failed to allocate memory for key-value pair.\n");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return -1;
        }

        // Generate random strings of the chosen lengths
        generate_random_alphanumeric(buffer_key, key_length);
        generate_random_alphanumeric(buffer_value, value_length);

        // Write directly to file
        if (!write_item_to_file(file, buffer_key, buffer_value))
        {
            printf("Error: Failed to write key-value pair to file.\n");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return -1;
        }

        printf("Inserted item %d / %d (key length: %d, value length: %d)\n",
               i + 1, INIT_DB_SIZE, key_length, value_length);

        // Free the temporary buffers
        free(buffer_key);
        free(buffer_value);
    }

    fclose(file);
    printf("Database initialization complete.\n");
    return 1;
}

/**
 * @brief Displays the current status of the in-memory cache
 *
 * This function prints detailed information about the cache including:
 * - Total number of items in cache
 * - For each cached item: key, value, hit count, and last access time
 *
 * @return int Returns 1 if cache is initialized and status is displayed,
 *             -1 if cache is not initialized
 */
int cache_status(void)
{
    if (!memory_cache)
    {
        printf("Cache is not initialized\n");
        return -1;
    }

    printf("Cache status: %d/%d items used\n", cache_size, CACHE_SIZE);
    for (int i = 0; i < cache_size; i++)
    {
        if (memory_cache[i].key)
        {
            printf("  [%d] Key: %s, Value: %s, Hits: %u, Last accessed: %u\n",
                   i, memory_cache[i].key, memory_cache[i].value,
                   memory_cache[i].hit_count, memory_cache[i].last_accessed);
        }
    }
    return 1;
}

/**
 * @brief Clears the terminal screen
 *
 * This function uses ANSI escape codes to clear the terminal screen.
 */
void clear(void)
{
    printf("\33[H\33[J"); // Clear the terminal screen
}