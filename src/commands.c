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

int zset_command(const char *key_to_set, const char *value_to_set)
{
    if (!ensure_database_exists())
    {
        return -1;
    }

    if (update_key_on_disk(key_to_set, value_to_set) < 0)
    {
        printf("Error: Failed to set key.\n");
        return -1;
    }

    add_to_cache(key_to_set, value_to_set);
    printf("OK\n");
    return 1;
}

char *zget_command(const char *key_to_get)
{
    DataItem *item = get_from_cache(key_to_get);
    if (item)
    {
#ifndef BENCHMARK_MODE
        printf("%s\n", item->value);
#endif
        return my_strdup(item->value);
    }

    char *value = NULL;
    int result = find_key_on_disk(key_to_get, &value);

    if (result < 0)
    {
#ifndef BENCHMARK_MODE
        printf("Error: Could not access data.\n");
#endif
        return NULL;
    }
    else if (result == 0)
    {
#ifndef BENCHMARK_MODE
        printf("Key '%s' not found.\n", key_to_get);
#endif
        return NULL;
    }

    add_to_cache(key_to_get, value);
#ifndef BENCHMARK_MODE
    printf("%s\n", value);
#endif
    return value;
}

int zrm_command(const char *key_to_remove)
{
    remove_from_cache(key_to_remove);

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

int zall_command()
{
    int result = print_all_data_from_disk();
    if (result <= 0)
    {
        printf("(empty or file error)\n");
        return -1;
    }
    return 1;
}

int init_db_command()
{
    srand(time(NULL));
    const int MIN_LENGTH = 4;
    const int MAX_LENGTH = 64;

    printf("Initializing database with %d random key-value pairs...\n", INIT_DB_SIZE);

    FILE *file = fopen(FILENAME, "wb");
    if (!file)
    {
        printf("Error: Could not open database file for writing.\n");
        return -1;
    }

    for (int i = 0; i < INIT_DB_SIZE; i++)
    {
        int key_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        int value_length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));

        char *buffer_key = malloc(key_length + 1);
        char *buffer_value = malloc(value_length + 1);

        if (!buffer_key || !buffer_value)
        {
            printf("Error: Failed to allocate memory for key-value pair.\n");
            free(buffer_key);
            free(buffer_value);
            fclose(file);
            return -1;
        }

        generate_random_alphanumeric(buffer_key, key_length);
        generate_random_alphanumeric(buffer_value, value_length);

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

        free(buffer_key);
        free(buffer_value);
    }

    fclose(file);
    printf("Database initialization complete.\n");
    return 1;
}

int cache_status(void)
{
    if (!memory_cache)
    {
        printf("Cache is not initialized\n");
        return -1;
    }

    printf("Cache status: %d items used\n", memory_cache->size);
    for (unsigned int i = 0; i < memory_cache->size; i++)
    {
        DataItem *item = memory_cache->table[i];
        while (item)
        {
            printf("  Key: %s, Value: %s, Hits: %u, Last accessed: %u\n",
                   item->key, item->value, item->hit_count, item->last_accessed);
            item = item->next;
        }
    }
    return 1;
}

#include "io_benchmark.h"

void clear(void)
{
    printf("\33[H\33[J");
}

int benchmark_command(void)
{
    printf("Starting benchmark with %d key-value pairs...\n", BENCHMARK_DB_SIZE);

    const char *benchmark_filename = "benchmark.zdb";
    double init_time, get_time;
    struct timespec start, end;

    // 1. Initialize temporary database
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (init_benchmark_db(benchmark_filename, BENCHMARK_DB_SIZE) != 0)
    {
        printf("Error: Failed to initialize benchmark database.\n");
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    init_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    printf("Database initialization complete in %.3fms\n", init_time);

    // 2. Get all keys from the benchmark database
    int num_keys = 0;
    char **keys = get_all_keys_from_benchmark_db(benchmark_filename, &num_keys);
    if (!keys || num_keys == 0)
    {
        printf("Error: No keys found in benchmark database.\n");
        cleanup_benchmark_db(benchmark_filename);
        return -1;
    }

    // 3. Perform zget operations and measure time
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < num_keys; i++)
    {
        char *value = zget_command(keys[i]);
        if (value) free(value); // Free the copied value from zget_command
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    get_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    printf("\nBenchmark Results:\n");
    printf("Total GET operations: %d\n", num_keys);
    printf("Total GET time: %.3fms\n", get_time);
    printf("Average GET time per key: %.3fus\n", (get_time * 1000.0) / num_keys);

    // Clean up keys array
    for (int i = 0; i < num_keys; i++)
    {
        free(keys[i]);
    }
    free(keys);

    // 4. Clean up temporary database
    cleanup_benchmark_db(benchmark_filename);
    printf("Benchmark database cleaned up.\n");

    return 0;
}
