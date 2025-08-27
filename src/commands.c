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
#include <stdbool.h> // For bool, true, false

int zset_command(const char *key_to_set, const char *value_to_set)
{
    if (!key_to_set || !value_to_set) {
        return CMD_EMPTY;
    }

    if (!ensure_database_exists())
    {
        return CMD_ERROR;
    }

    if (strlen(key_to_set) == 0 || strlen(value_to_set) == 0) {
        return CMD_EMPTY;
    }

    if (update_key_on_disk(key_to_set, value_to_set) < 0)
    {
        return CMD_ERROR;
    }

    add_to_cache(key_to_set, value_to_set);
    return CMD_SUCCESS;
}

int zget_command(const char *key_to_get, char **result_value)
{
    if (!key_to_get || strlen(key_to_get) == 0) {
        return CMD_EMPTY;
    }

    *result_value = NULL; // Initialize to NULL

    DataItem *item = get_from_cache(key_to_get);
    if (item)
    {
        *result_value = my_strdup(item->value);
        if (!*result_value) {
            return CMD_ERROR; // Memory allocation failed
        }
        return CMD_SUCCESS;
    }

    char *value = NULL;
    int result = find_key_on_disk(key_to_get, &value);

    if (result < 0)
    {
        free(value); // Clean up in case of error
        return CMD_ERROR;
    }
    else if (result == 0)
    {
        free(value); // Clean up when key not found
        return CMD_NOT_FOUND;
    }

    // Only add to cache if we successfully retrieved the value
    add_to_cache(key_to_get, value);
    *result_value = value;
    return CMD_SUCCESS;
}

int zrm_command(const char *key_to_remove)
{
    if (!key_to_remove || strlen(key_to_remove) == 0) {
        return CMD_EMPTY;
    }

    remove_from_cache(key_to_remove);

    int result = remove_key_from_disk(key_to_remove);

    if (result < 0)
    {
        return CMD_ERROR;
    }
    else if (result == 0)
    {
        return CMD_NOT_FOUND;
    }
    else
    {
        return CMD_SUCCESS;
    }
}

int zall_command()
{
    int result = print_all_data_from_disk();
    if (result <= 0)
    {
        return CMD_ERROR;
    }
    return CMD_SUCCESS;
}

int init_db_command()
{
    srand(time(NULL));
    const int MIN_LENGTH = 4;
    const int MAX_LENGTH = 64;

    FILE *file = fopen(FILENAME, "wb");
    if (!file)
    {
        return CMD_ERROR;
    }

    for (int i = 0; i < INIT_DB_SIZE; i++)
    {
        int key_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        int value_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));

        char *buffer_key = malloc(key_length + 1);
        char *buffer_value = malloc(value_length + 1);

        if (!buffer_key || !buffer_value)
        {
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return CMD_ERROR;
        }

        generate_random_alphanumeric(buffer_key, key_length);
        generate_random_alphanumeric(buffer_value, value_length);

        if (!write_item_to_file(file, buffer_key, buffer_value))
        {
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return CMD_ERROR;
        }

        free(buffer_key);
        free(buffer_value);
    }

    fclose(file);
    return CMD_SUCCESS;
}

int cache_status(void)
{
    if (!memory_cache)
    {
        return CMD_ERROR;
    }

    return CMD_SUCCESS;
}

#include "io_benchmark.h"

void clear(void)
{
    printf("\33[H\33[J");
}

int benchmark_command(void)
{
    const char *benchmark_filename = "benchmark.zdb";
    double init_time, get_time;
    struct timespec start, end;
    char **keys = NULL;
    int num_keys = 0;

    // 1. Initialize temporary database
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (init_benchmark_db(benchmark_filename, BENCHMARK_DB_SIZE) != 0)
    {
        return CMD_ERROR;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    init_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // 2. Get all keys from the benchmark database
    keys = get_all_keys_from_benchmark_db(benchmark_filename, &num_keys);
    if (!keys || num_keys == 0)
    {
        cleanup_benchmark_db(benchmark_filename);
        return CMD_ERROR;
    }

    // 3. Perform zget operations and measure time
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < num_keys; i++)
    {
        if (!keys[i]) continue; // Skip NULL keys (defensive programming)
        char *value;
        int result = zget_command(keys[i], &value);
        if (result == CMD_SUCCESS && value) {
            free(value);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    get_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // Free keys
    for (int i = 0; i < num_keys; i++) {
        free(keys[i]);
    }
    free(keys);

    // 4. Clean up temporary database
    cleanup_benchmark_db(benchmark_filename);

    // 5. Print benchmark recap with metrics
    printf("\n=== BENCHMARK RECAP ===\n");
    printf("Database Operations:\n");
    printf("  • Database initialization: %.2f ms\n", init_time);
    printf("  • Total keys processed: %d\n", num_keys);
    printf("  • Get operations time: %.2f ms\n", get_time);
    printf("\nPerformance Metrics:\n");
    printf("  • Average time per get operation: %.4f ms\n", get_time / num_keys);
    printf("  • Get operations per second: %.2f ops/sec\n", (num_keys * 1000.0) / get_time);
    printf("  • Total benchmark time: %.2f ms\n", init_time + get_time);
    printf("  • Database size: %d entries\n", BENCHMARK_DB_SIZE);

    printf("================================\n\n");

    return CMD_SUCCESS;
}
