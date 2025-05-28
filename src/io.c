#include "io.h"
#include "config.h"
#include "ds.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global flag to track if hit counts have been reset this session
static int hit_counts_reset = 0;

int load_all_data_from_disk(DataItem **full_data_list, size_t *list_size, size_t *list_capacity)
{
    free_data_list(full_data_list, list_size, list_capacity); // Clear the existing list

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        // perror("fopen for read (rb)"); // Optional: log if the file doesn't exist
        return 1; // File not found, that's okay
    }

    DataItem item;
    while (1)
    {
        size_t key_len, value_len;

        if (fread(&key_len, sizeof(size_t), 1, file) != 1)
            break;
        item.key = malloc(key_len + 1);
        if (!item.key || fread(item.key, 1, key_len, file) != key_len)
        {
            perror("Error reading key from disk");
            free(item.key);
            fclose(file);
            return 0; // Error
        }
        item.key[key_len] = '\0';

        if (fread(&value_len, sizeof(size_t), 1, file) != 1)
        {
            perror("Error reading value_len");
            free(item.key);
            fclose(file);
            return 0; // Error
        }
        item.value = malloc(value_len + 1);
        if (!item.value || fread(item.value, 1, value_len, file) != value_len)
        {
            perror("Error reading value from disk");
            free(item.key);
            free(item.value);
            fclose(file);
            return 0; // Error
        }
        item.value[value_len] = '\0';

        if (fread(&item.hit_count, sizeof(unsigned int), 1, file) != 1)
        {
            perror("Error reading hit_count");
            free(item.key);
            free(item.value);
            fclose(file);
            return 0; // Error
        }

        ensure_list_capacity(full_data_list, list_capacity, *list_size + 1);

        // Make copies of the strings before storing in the array
        (*full_data_list)[*list_size].key = my_strdup(item.key);
        (*full_data_list)[*list_size].value = my_strdup(item.value);
        (*full_data_list)[*list_size].hit_count = item.hit_count;

        if (!(*full_data_list)[*list_size].key || !(*full_data_list)[*list_size].value)
        {
            perror("Failed to allocate memory for data list entry");
            free_data_item_contents(&(*full_data_list)[*list_size]);
            fclose(file);
            return 0; // Error
        }

        (*list_size)++;

        // Free the temporary buffers
        free(item.key);
        free(item.value);
    }

    if (ferror(file))
    {
        perror("File read error during full load");
        fclose(file);
        return 0; // Error
    }
    fclose(file);
    return 1; // Success
}

void save_all_data_to_disk(DataItem *data_list, size_t list_size)
{
    FILE *file = fopen(FILENAME, "wb");
    if (file == NULL)
    {
        perror("Failed to open file for writing all data");
        return;
    }

    for (size_t i = 0; i < list_size; i++)
    {
        size_t key_len = strlen(data_list[i].key);
        size_t value_len = strlen(data_list[i].value);

        if (fwrite(&key_len, sizeof(size_t), 1, file) != 1 ||
            fwrite(data_list[i].key, 1, key_len, file) != key_len ||
            fwrite(&value_len, sizeof(size_t), 1, file) != 1 ||
            fwrite(data_list[i].value, 1, value_len, file) != value_len ||
            fwrite(&data_list[i].hit_count, sizeof(unsigned int), 1, file) != 1)
        {
            perror("Error writing data to disk file");
            fclose(file);
            return;
        }
    }

    if (fclose(file) != 0)
    {
        perror("Failed to close file after writing all data");
    }
}

int print_all_data_from_disk(void)
{
    // Reset hit counts if this is the first disk access
    reset_hit_counts_in_file();

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        printf("(empty)\n");
        return 1; // File not found is okay
    }

    int key_count = 0;
    while (1)
    {
        size_t key_len, value_len;
        char *key = NULL;
        char *value = NULL;
        unsigned int hit_count;
        int is_hot = 0;

        // Read key length and key
        if (fread(&key_len, sizeof(size_t), 1, file) != 1)
            break;
        key = malloc(key_len + 1);
        if (!key || fread(key, 1, key_len, file) != key_len)
        {
            perror("Error reading key from disk");
            free(key);
            fclose(file);
            return 0; // Error
        }
        key[key_len] = '\0';

        // Read value length and value
        if (fread(&value_len, sizeof(size_t), 1, file) != 1)
        {
            perror("Error reading value_len");
            free(key);
            fclose(file);
            return 0;
        }
        value = malloc(value_len + 1);
        if (!value || fread(value, 1, value_len, file) != value_len)
        {
            perror("Error reading value from disk");
            free(key);
            free(value);
            fclose(file);
            return 0;
        }
        value[value_len] = '\0';

        // Read hit count from disk
        if (fread(&hit_count, sizeof(unsigned int), 1, file) != 1)
        {
            perror("Error reading hit_count");
            free(key);
            free(value);
            fclose(file);
            return 0;
        }

        // Check if key exists in memory cache and use the higher hit count
        for (size_t i = 0; i < cache_size; i++)
        {
            if (strcmp(memory_cache[i].key, key) == 0)
            {
                // Use the higher hit count between disk and cache
                if (memory_cache[i].hit_count > hit_count)
                {
                    hit_count = memory_cache[i].hit_count;
                }
                break;
            }
        }

        // Determine HOT/COLD status based on hit count threshold
        is_hot = (hit_count > HIT_THRESHOLD_FOR_CACHING);

        // Print the item with appropriate status
        printf("%s:%s [%s, hits: %u]\n", key, value, is_hot ? "HOT" : "COLD", hit_count);
        key_count++;

        // Free temporary buffers
        free(key);
        free(value);
    }

    if (ferror(file))
    {
        perror("File read error during print");
        fclose(file);
        return 0;
    }

    fclose(file);
    printf("Total keys: %d\n", key_count);
    if (key_count == 0)
    {
        printf("(empty)\n");
    }

    return 1;
}

// Helper function to write a single item to file
int write_item_to_file(FILE *file, const char *key, const char *value, unsigned int hit_count)
{
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);

    if (fwrite(&key_len, sizeof(size_t), 1, file) != 1 ||
        fwrite(key, 1, key_len, file) != key_len ||
        fwrite(&value_len, sizeof(size_t), 1, file) != 1 ||
        fwrite(value, 1, value_len, file) != value_len ||
        fwrite(&hit_count, sizeof(unsigned int), 1, file) != 1)
    {
        return 0; // Error
    }
    return 1; // Success
}

// Helper function to read a single item from file
static int read_item_from_file(FILE *file, char **key, char **value, unsigned int *hit_count)
{
    size_t key_len, value_len;

    // Read key length and key
    if (fread(&key_len, sizeof(size_t), 1, file) != 1)
        return 0; // End of file or error
    *key = malloc(key_len + 1);
    if (!*key || fread(*key, 1, key_len, file) != key_len)
    {
        free(*key);
        return -1; // Error
    }
    (*key)[key_len] = '\0';

    // Read value length and value
    if (fread(&value_len, sizeof(size_t), 1, file) != 1)
    {
        free(*key);
        return -1;
    }
    *value = malloc(value_len + 1);
    if (!*value || fread(*value, 1, value_len, file) != value_len)
    {
        free(*key);
        free(*value);
        return -1;
    }
    (*value)[value_len] = '\0';

    // Read hit count
    if (fread(hit_count, sizeof(unsigned int), 1, file) != 1)
    {
        free(*key);
        free(*value);
        return -1;
    }

    return 1; // Success
}

void reset_hit_counts_in_file(void)
{
    if (hit_counts_reset)
    {
        return; // Already reset in this session
    }

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        // File doesn't exist yet, that's okay
        hit_counts_reset = 1;
        return;
    }

    FILE *temp_file = fopen(FILENAME ".tmp", "wb");
    if (temp_file == NULL)
    {
        perror("Failed to create temporary file for hit count reset");
        fclose(file);
        return;
    }

    char *key = NULL;
    char *value = NULL;
    unsigned int hit_count;
    int success = 1;

    while (1)
    {
        int result = read_item_from_file(file, &key, &value, &hit_count);
        if (result <= 0)
        {
            if (result == 0)
                break; // End of file
            success = 0;
            break;
        }

        // Write the item with hit count reset to 0
        if (!write_item_to_file(temp_file, key, value, 0))
        {
            success = 0;
            break;
        }

        free(key);
        free(value);
    }

    fclose(file);
    fclose(temp_file);

    if (success)
    {
        if (rename(FILENAME ".tmp", FILENAME) != 0)
        {
            perror("Failed to replace file with reset hit counts");
            remove(FILENAME ".tmp");
            return;
        }
        hit_counts_reset = 1;
    }
    else
    {
        remove(FILENAME ".tmp");
    }
}

// Modify find_key_on_disk to properly handle hit counts
int find_key_on_disk(const char *key, char **value, unsigned int *hit_count)
{
    // Reset hit counts if this is the first disk access
    reset_hit_counts_in_file();

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
        return 0; // File not found

    int found = 0;
    char *current_key = NULL;
    char *current_value = NULL;
    unsigned int current_hit_count;
    unsigned int new_hit_count = 1; // Start at 1 for first access

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value, &current_hit_count);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            fclose(file);
            return -1;
        }

        if (strcmp(current_key, key) == 0)
        {
            *value = current_value;
            // If this is the first access after reset, use 1, otherwise increment
            new_hit_count = (current_hit_count == 0) ? 1 : current_hit_count + 1;
            *hit_count = new_hit_count; // Return the appropriate count
            found = 1;
            free(current_key);
            break;
        }

        free(current_key);
        free(current_value);
    }

    fclose(file);

    // If we found the key, update its hit count in the file
    if (found)
    {
        if (update_key_on_disk(key, *value, new_hit_count) < 0)
        {
            // If update fails, we still return the value but log a warning
            printf("\nWarning: Failed to update hit count on disk.\n");
        }
    }

    return found;
}

int update_key_on_disk(const char *key, const char *new_value, unsigned int new_hit_count)
{
    FILE *temp_file = fopen(FILENAME ".tmp", "wb");
    if (temp_file == NULL)
        return 0;

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        fclose(temp_file);
        return 0;
    }

    int found = 0;
    char *current_key = NULL;
    char *current_value = NULL;
    unsigned int current_hit_count;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value, &current_hit_count);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            fclose(file);
            fclose(temp_file);
            remove(FILENAME ".tmp");
            return -1;
        }

        if (strcmp(current_key, key) == 0)
        {
            // Write updated item
            if (!write_item_to_file(temp_file, key, new_value, new_hit_count))
            {
                free(current_key);
                free(current_value);
                fclose(file);
                fclose(temp_file);
                remove(FILENAME ".tmp");
                return -1;
            }
            found = 1;
        }
        else
        {
            // Write unchanged item
            if (!write_item_to_file(temp_file, current_key, current_value, current_hit_count))
            {
                free(current_key);
                free(current_value);
                fclose(file);
                fclose(temp_file);
                remove(FILENAME ".tmp");
                return -1;
            }
        }

        free(current_key);
        free(current_value);
    }

    fclose(file);
    fclose(temp_file);

    if (!found)
    {
        remove(FILENAME ".tmp");
        return 0;
    }

    // Replace original file with temp file
    if (rename(FILENAME ".tmp", FILENAME) != 0)
    {
        remove(FILENAME ".tmp");
        return -1;
    }

    return 1;
}

int remove_key_from_disk(const char *key)
{
    FILE *temp_file = fopen(FILENAME ".tmp", "wb");
    if (temp_file == NULL)
        return 0;

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        fclose(temp_file);
        return 0;
    }

    int found = 0;
    char *current_key = NULL;
    char *current_value = NULL;
    unsigned int current_hit_count;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value, &current_hit_count);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            fclose(file);
            fclose(temp_file);
            remove(FILENAME ".tmp");
            return -1;
        }

        if (strcmp(current_key, key) != 0)
        {
            // Write item if it's not the one to remove
            if (!write_item_to_file(temp_file, current_key, current_value, current_hit_count))
            {
                free(current_key);
                free(current_value);
                fclose(file);
                fclose(temp_file);
                remove(FILENAME ".tmp");
                return -1;
            }
        }
        else
        {
            found = 1;
        }

        free(current_key);
        free(current_value);
    }

    fclose(file);
    fclose(temp_file);

    if (!found)
    {
        remove(FILENAME ".tmp");
        return 0;
    }

    // Replace original file with temp file
    if (rename(FILENAME ".tmp", FILENAME) != 0)
    {
        remove(FILENAME ".tmp");
        return -1;
    }

    return 1;
}

int append_key_to_disk(const char *key, const char *value, unsigned int hit_count)
{
    FILE *file = fopen(FILENAME, "ab"); // Open for append
    if (file == NULL)
    {
        // If file doesn't exist, try to create it
        file = fopen(FILENAME, "wb");
        if (file == NULL)
            return 0;
    }

    int success = write_item_to_file(file, key, value, hit_count);
    fclose(file);
    return success;
}