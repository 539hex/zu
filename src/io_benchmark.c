#include "io_benchmark.h"
#include "io.h"
#include "utils.h"
#include "ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Helper function to write a single item to a specified file
static int write_item_to_benchmark_file(FILE *file, const char *key, const char *value) {
    // Re-using the write_item_to_file from io.h
    return write_item_to_file(file, key, value);
}

// Helper function to read a single item from a specified file
static int read_item_from_benchmark_file(FILE *file, char **key, char **value) {
    // Re-using the read_item_from_file from io.h
    return read_item_from_file(file, key, value);
}

int init_benchmark_db(const char *filename, int num_entries)
{
    srand(time(NULL));
    const int MIN_LENGTH = 4;
    const int MAX_LENGTH = 64;

    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        perror("Error opening benchmark database file for writing");
        return -1;
    }

    for (int i = 0; i < num_entries; i++)
    {
        int key_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        int value_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));

        char *buffer_key = malloc(key_length + 1);
        char *buffer_value = malloc(value_length + 1);

        if (!buffer_key || !buffer_value)
        {
            perror("Failed to allocate memory for benchmark key-value pair");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return -1;
        }

        generate_random_alphanumeric(buffer_key, key_length);
        generate_random_alphanumeric(buffer_value, value_length);

        if (!write_item_to_benchmark_file(file, buffer_key, buffer_value))
        {
            perror("Failed to write benchmark key-value pair to file");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return -1;
        }

        free(buffer_key);
        free(buffer_value);
    }

    fclose(file);
    return 0;
}

char *find_key_in_benchmark_db(const char *filename, const char *key)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        return NULL; // File error or not found
    }

    char *current_key = NULL;
    char *current_value = NULL;
    char *found_value = NULL;

    while (read_item_from_benchmark_file(file, &current_key, &current_value) > 0)
    {
        if (strcmp(current_key, key) == 0)
        {
            found_value = my_strdup(current_value);
            free(current_key);
            free(current_value);
            break;
        }
        free(current_key);
        free(current_value);
    }

    fclose(file);
    return found_value;
}

char **get_all_keys_from_benchmark_db(const char *filename, int *num_keys)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        *num_keys = 0;
        return NULL;
    }

    char **keys = NULL;
    int count = 0;
    int capacity = 10;
    keys = malloc(sizeof(char *) * capacity);
    if (!keys) {
        perror("Failed to allocate memory for keys");
        *num_keys = 0;
        fclose(file);
        return NULL;
    }

    char *current_key = NULL;
    char *current_value = NULL;

    while (read_item_from_benchmark_file(file, &current_key, &current_value) > 0)
    {
        if (count >= capacity) {
            capacity *= 2;
            keys = realloc(keys, sizeof(char *) * capacity);
            if (!keys) {
                perror("Failed to reallocate memory for keys");
                // Free already allocated keys
                for (int i = 0; i < count; i++) {
                    free(keys[i]);
                }
                *num_keys = 0;
                fclose(file);
                return NULL;
            }
        }
        keys[count++] = my_strdup(current_key);
        free(current_key);
        free(current_value);
    }

    fclose(file);
    *num_keys = count;
    return keys;
}

void cleanup_benchmark_db(const char *filename)
{
    unlink(filename);
}
