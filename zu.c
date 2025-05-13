#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace
#include <time.h>  // For clock_gettime

#define FILENAME "z.u"
#define INITIAL_CAPACITY 10
#define MAX_LINE_LENGTH 2048

// --- Global Configuration ---
unsigned int HIT_THRESHOLD_FOR_CACHING = 2; // Keys with hits >= this value are cached

// --- Data Structures ---
typedef struct
{
    char *key;
    char *value;
    unsigned int hit_count;
} DataItem;

// In-memory cache for "hot" elements
DataItem *memory_cache = NULL;
size_t cache_size = 0;
size_t cache_capacity = 0;

// --- Timer Functions ---
struct timespec timer_start_ts;

void start_timer()
{
    clock_gettime(CLOCK_MONOTONIC, &timer_start_ts);
}

double end_timer()
{
    struct timespec timer_end_ts;
    clock_gettime(CLOCK_MONOTONIC, &timer_end_ts);
    long seconds = timer_end_ts.tv_sec - timer_start_ts.tv_sec;
    long nanoseconds = timer_end_ts.tv_nsec - timer_start_ts.tv_nsec;
    return (double)seconds * 1000.0 + (double)nanoseconds / 1000000.0;
}

// --- Helper Functions ---
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

void free_data_list(DataItem **list, size_t *size, size_t *capacity)
{
    if (*list != NULL)
    {
        for (size_t i = 0; i < *size; i++)
        {
            free_data_item_contents(&(*list)[i]);
        }
        free(*list);
        *list = NULL;
    }
    *size = 0;
    *capacity = 0;
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
            exit(EXIT_FAILURE);
        }
        *list = new_data_list;
        *capacity = new_capacity;
    }
}

// Adds or updates an element in the in-memory cache
void add_or_update_in_memory_cache(const char *key, const char *value, unsigned int hit_count)
{
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key) == 0)
        {
            free(memory_cache[i].value);
            memory_cache[i].value = my_strdup(value);
            memory_cache[i].hit_count = hit_count;
            return;
        }
    }
    // Not found, add new
    ensure_list_capacity(&memory_cache, &cache_capacity, cache_size + 1);
    memory_cache[cache_size].key = my_strdup(key);
    memory_cache[cache_size].value = my_strdup(value);
    memory_cache[cache_size].hit_count = hit_count;
    if (!memory_cache[cache_size].key || !memory_cache[cache_size].value)
    {
        perror("Failed to allocate memory for new cache entry");
        free_data_item_contents(&memory_cache[cache_size]); // Clean partially allocated
        return;                                             // Do not increment cache_size
    }
    cache_size++;
}

void remove_from_memory_cache(const char *key)
{
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key) == 0)
        {
            free_data_item_contents(&memory_cache[i]);
            // Move the last element to this position to fill the gap
            if (i < cache_size - 1)
            {
                memory_cache[i] = memory_cache[cache_size - 1];
            }
            cache_size--;
            // Optional: resize buffer if cache_size is much smaller than cache_capacity
            return;
        }
    }
}

// --- Core Data Handling ---

// Loads all data from the file into a temporary list
int load_all_data_from_disk(DataItem **full_data_list, size_t *list_size, size_t *list_capacity)
{
    free_data_list(full_data_list, list_size, list_capacity); // Clear the existing list

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
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
            return 0;
        }
        item.key[key_len] = '\0';

        if (fread(&value_len, sizeof(size_t), 1, file) != 1)
        {
            perror("Error reading value_len");
            free(item.key);
            fclose(file);
            return 0;
        }
        item.value = malloc(value_len + 1);
        if (!item.value || fread(item.value, 1, value_len, file) != value_len)
        {
            perror("Error reading value from disk");
            free(item.key);
            free(item.value);
            fclose(file);
            return 0;
        }
        item.value[value_len] = '\0';

        // Read hit_count from disk during runtime
        if (fread(&item.hit_count, sizeof(unsigned int), 1, file) != 1)
        {
            perror("Error reading hit_count");
            free(item.key);
            free(item.value);
            fclose(file);
            return 0;
        }

        ensure_list_capacity(full_data_list, list_capacity, *list_size + 1);
        (*full_data_list)[*list_size] = item; // Copy the struct
        (*list_size)++;
    }

    if (ferror(file))
        perror("File read error during full load");
    fclose(file);
    return 1;
}

// Saves all data from a list (typically the full list) to disk
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
        perror("Failed to close file after writing all data");
}

// Rebuilds the in-memory cache based on the full data list and hit threshold
void rebuild_memory_cache(DataItem *full_data_list, size_t list_size)
{
    free_data_list(&memory_cache, &cache_size, &cache_capacity);
    for (size_t i = 0; i < list_size; i++)
    {
        if (full_data_list[i].hit_count >= HIT_THRESHOLD_FOR_CACHING)
        {
            add_or_update_in_memory_cache(full_data_list[i].key, full_data_list[i].value, full_data_list[i].hit_count);
        }
    }
}

void load_initial_cache()
{
    // Load data and reset hit counts to 0 at startup
    DataItem *disk_data = NULL;
    size_t disk_data_size = 0;
    size_t disk_data_capacity = 0;
    if (load_all_data_from_disk(&disk_data, &disk_data_size, &disk_data_capacity))
    {
        // Reset hit counts to 0
        for (size_t i = 0; i < disk_data_size; i++)
        {
            disk_data[i].hit_count = 0;
        }
        // Save reset hit counts to disk
        save_all_data_to_disk(disk_data, disk_data_size);
        // Clear cache (no keys will be HOT at startup)
        free_data_list(&memory_cache, &cache_size, &cache_capacity);
    }
    free_data_list(&disk_data, &disk_data_size, &disk_data_capacity);
}

// Save all data to disk at program exit
void save_all_data_at_exit()
{
    DataItem *full_data_list = NULL;
    size_t full_list_size = 0;
    size_t full_list_capacity = 0;

    if (load_all_data_from_disk(&full_data_list, &full_list_size, &full_list_capacity))
    {
        // Synchronize hit counts from cache
        for (size_t i = 0; i < full_list_size; i++)
        {
            for (size_t j = 0; j < cache_size; j++)
            {
                if (strcmp(full_data_list[i].key, memory_cache[j].key) == 0)
                {
                    full_data_list[i].hit_count = memory_cache[j].hit_count;
                    break;
                }
            }
        }
        save_all_data_to_disk(full_data_list, full_list_size);
    }
    free_data_list(&full_data_list, &full_list_size, &full_list_capacity);
}

// --- Command Functions ---

// zset command to set a key-value pair
void zset_command(const char *key_to_set, const char *value_to_set)
{
    DataItem *full_data_list = NULL;
    size_t full_list_size = 0;
    size_t full_list_capacity = 0;
    int found_in_full_list = 0;
    unsigned int new_hit_count = 1; // Default for a new key or set

    load_all_data_from_disk(&full_data_list, &full_list_size, &full_list_capacity);

    // Synchronize hit counts from cache to full_data_list and find the key to set
    for (size_t i = 0; i < full_list_size; i++)
    {
        for (size_t j = 0; j < cache_size; j++)
        {
            if (strcmp(full_data_list[i].key, memory_cache[j].key) == 0)
            {
                full_data_list[i].hit_count = memory_cache[j].hit_count; // Update hit
                break;
            }
        }
        if (strcmp(full_data_list[i].key, key_to_set) == 0)
        {
            free(full_data_list[i].value);
            full_data_list[i].value = my_strdup(value_to_set);
            int was_in_cache = 0;
            for (size_t k = 0; k < cache_size; ++k)
            {
                if (strcmp(memory_cache[k].key, key_to_set) == 0)
                {
                    new_hit_count = memory_cache[k].hit_count + 1; // zset counts as a hit
                    was_in_cache = 1;
                    break;
                }
            }
            if (!was_in_cache)
            {
                new_hit_count = full_data_list[i].hit_count + 1;
            }
            full_data_list[i].hit_count = new_hit_count;
            found_in_full_list = 1;
        }
    }

    if (!found_in_full_list)
    {
        ensure_list_capacity(&full_data_list, &full_list_capacity, full_list_size + 1);
        full_data_list[full_list_size].key = my_strdup(key_to_set);
        full_data_list[full_list_size].value = my_strdup(value_to_set);
        full_data_list[full_list_size].hit_count = 1; // Initial hit for new key
        if (!full_data_list[full_list_size].key || !full_data_list[full_list_size].value)
        {
            perror("Failed to allocate for new entry in zset (full_list)");
            free_data_item_contents(&full_data_list[full_list_size]);
            free_data_list(&full_data_list, &full_list_size, &full_list_capacity);
            printf("Error: Failed to set key.");
            return;
        }
        full_list_size++;
    }

    save_all_data_to_disk(full_data_list, full_list_size);
    rebuild_memory_cache(full_data_list, full_list_size);

    const char *status = "COLD";
    unsigned int final_hits = 1;
    for (size_t i = 0; i < full_list_size; i++)
    {
        if (strcmp(full_data_list[i].key, key_to_set) == 0)
        {
            status = (full_data_list[i].hit_count >= HIT_THRESHOLD_FOR_CACHING) ? "HOT" : "COLD";
            final_hits = full_data_list[i].hit_count;
            break;
        }
    }

    free_data_list(&full_data_list, &full_list_size, &full_list_capacity);
    printf("OK [%s, hits: %u]", status, final_hits);
}

// zget command to retrieve a key's value
void zget_command(const char *key_to_get)
{
    // Search in cache
    for (size_t i = 0; i < cache_size; i++)
    {
        if (strcmp(memory_cache[i].key, key_to_get) == 0)
        {
            memory_cache[i].hit_count++;
            printf("Value: %s [HOT, hits: %u]", memory_cache[i].value, memory_cache[i].hit_count);
            return;
        }
    }

    // Not in cache, search on disk
    DataItem *full_data_list = NULL;
    size_t full_list_size = 0;
    size_t full_list_capacity = 0;
    int found_key = 0;
    size_t found_index = 0;

    if (!load_all_data_from_disk(&full_data_list, &full_list_size, &full_list_capacity))
    {
        printf("Key '%s' not found (file error).", key_to_get);
        return;
    }

    // Synchronize cache hit counts to full_data_list
    for (size_t i = 0; i < full_list_size; i++)
    {
        for (size_t j = 0; j < cache_size; j++)
        {
            if (strcmp(full_data_list[i].key, memory_cache[j].key) == 0)
            {
                full_data_list[i].hit_count = memory_cache[j].hit_count;
                break;
            }
        }
    }

    // Search for the key and update hit count
    for (size_t i = 0; i < full_list_size; i++)
    {
        if (strcmp(full_data_list[i].key, key_to_get) == 0)
        {
            full_data_list[i].hit_count++; // Increment hit count
            found_key = 1;
            found_index = i;

            // Add to cache if it reaches the threshold
            if (full_data_list[i].hit_count >= HIT_THRESHOLD_FOR_CACHING)
            {
                printf("DEBUG: Adding %s to cache with hit_count: %u\n", full_data_list[i].key, full_data_list[i].hit_count);
                add_or_update_in_memory_cache(
                    full_data_list[i].key,
                    full_data_list[i].value,
                    full_data_list[i].hit_count);
            }
            break;
        }
    }

    // Handle response
    if (found_key)
    {
        // Check cache for HOT keys
        printf("DEBUG: Checking cache for %s\n", key_to_get);
        for (size_t i = 0; i < cache_size; i++)
        {
            if (strcmp(memory_cache[i].key, key_to_get) == 0 && (memory_cache[i].hit_count != HIT_THRESHOLD_FOR_CACHING))
            {
                printf("DEBUG: Found %s in cache\n", key_to_get);
                printf("Value: %s [HOT, hits: %u]", memory_cache[i].value, memory_cache[i].hit_count);
                free_data_list(&full_data_list, &full_list_size, &full_list_capacity);
                return;
            }
        }

        // If not in cache, use full_data_list
        printf("DEBUG: %s not in cache, using disk data\n", key_to_get);
        const char *status = (full_data_list[found_index].hit_count >= HIT_THRESHOLD_FOR_CACHING) && (full_data_list[found_index].hit_count != HIT_THRESHOLD_FOR_CACHING) ? "HOT" : "COLD";
        printf("Value: %s [%s, hits: %u]", full_data_list[found_index].value, status, full_data_list[found_index].hit_count);
        save_all_data_to_disk(full_data_list, full_list_size);
    }
    else
    {
        printf("Key '%s' not found.", key_to_get);
    }

    free_data_list(&full_data_list, &full_list_size, &full_list_capacity);
}

// zrm command to remove a key
void zrm_command(const char *key_to_remove)
{
    int found = 0;

    // Remove from in-memory cache if present
    remove_from_memory_cache(key_to_remove);

    // Load all data from disk
    DataItem *full_data_list = NULL;
    size_t full_list_size = 0;
    size_t full_list_capacity = 0;
    if (!load_all_data_from_disk(&full_data_list, &full_list_size, &full_list_capacity))
    {
        printf("Key '%s' not found (file error).", key_to_remove);
        return;
    }

    // Create a new list excluding the key to remove
    DataItem *new_data_list = NULL;
    size_t new_list_size = 0;
    size_t new_list_capacity = 0;

    for (size_t i = 0; i < full_list_size; i++)
    {
        if (strcmp(full_data_list[i].key, key_to_remove) == 0)
        {
            found = 1;
            free_data_item_contents(&full_data_list[i]);
            continue;
        }
        ensure_list_capacity(&new_data_list, &new_list_capacity, new_list_size + 1);
        new_data_list[new_list_size] = full_data_list[i]; // Transfer ownership
        new_list_size++;
    }

    // Save the new list to disk
    save_all_data_to_disk(new_data_list, new_list_size);

    // Rebuild the cache based on the new data
    rebuild_memory_cache(new_data_list, new_list_size);

    // Clean up
    free(new_data_list); // Only free the array, not the contents (transferred)
    free_data_list(&full_data_list, &full_list_size, &full_list_capacity);

    // Provide feedback
    if (found)
    {
        printf("OK: Key '%s' removed.", key_to_remove);
    }
    else
    {
        printf("Key '%s' not found.", key_to_remove);
    }
}

// zall command to list all key-value pairs
void zall_command()
{
    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        printf("(empty or file error)\n");
        return;
    }

    int count = 0;
    DataItem item;
    while (1)
    {
        size_t key_len, value_len;
        unsigned int hit_count;

        if (fread(&key_len, sizeof(size_t), 1, file) != 1)
            break;
        item.key = malloc(key_len + 1);
        if (!item.key || fread(item.key, 1, key_len, file) != key_len)
        {
            free(item.key);
            break;
        }
        item.key[key_len] = '\0';

        if (fread(&value_len, sizeof(size_t), 1, file) != 1)
        {
            free(item.key);
            break;
        }
        item.value = malloc(value_len + 1);
        if (!item.value || fread(item.value, 1, value_len, file) != value_len)
        {
            free(item.key);
            free(item.value);
            break;
        }
        item.value[value_len] = '\0';

        // Read hit_count from disk
        if (fread(&hit_count, sizeof(unsigned int), 1, file) != 1)
        {
            free(item.key);
            free(item.value);
            break;
        }

        // Use cache hit count if available
        unsigned int display_hit_count = hit_count;
        for (size_t i = 0; i < cache_size; ++i)
        {
            if (strcmp(memory_cache[i].key, item.key) == 0)
            {
                display_hit_count = memory_cache[i].hit_count;
                break;
            }
        }

        // Determine status based on hit count threshold
        const char *status = (display_hit_count >= HIT_THRESHOLD_FOR_CACHING) ? "HOT" : "COLD";

        // Print key, value, hit count, and status
        printf("%s:%s %u %s\n",
               item.key,
               item.value,
               display_hit_count,
               status);

        count++;
        free_data_item_contents(&item);
    }

    fclose(file);
    if (count == 0 && !feof(file))
    {
        perror("Error during zall read, or file is empty/corrupt");
    }
    else if (count == 0)
    {
        printf("(empty)\n");
    }
}

int main()
{
    char line[MAX_LINE_LENGTH];
    char *command_token, *key_token, *value_token;
    double exec_time;

    load_initial_cache(); // Load initial cache with reset hit counts

    printf("Zu 0.0.1 - Key-Value Database with Memory Cache\n");
    printf("HIT_THRESHOLD=%u (keys with this many hits are kept in memory)\n", HIT_THRESHOLD_FOR_CACHING);
    printf("Type 'exit' or 'quit' to exit\n");
    printf("Commands: zset <key> <value>, zget <key>, zrm <key>, zall\n\n");

    while (1)
    {
        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            if (feof(stdin))
                printf("\nExiting.\n");
            else
                perror("fgets error");
            break;
        }
        line[strcspn(line, "\n\r")] = 0;
        command_token = strtok(line, " \t");
        if (command_token == NULL || *command_token == '\0')
            continue;

        start_timer(); // Start timer before executing the command

        if (strcmp(command_token, "zset") == 0)
        {
            key_token = strtok(NULL, " \t");
            value_token = strtok(NULL, ""); // The rest is the value
            if (key_token && value_token)
            {
                while (*value_token && isspace((unsigned char)*value_token))
                    value_token++; // Trim leading spaces
                if (*value_token)
                {
                    zset_command(key_token, value_token);
                }
                else
                {
                    printf("Usage: zset <key> <value>");
                }
            }
            else
            {
                printf("Usage: zset <key> <value>");
            }
        }
        else if (strcmp(command_token, "zget") == 0)
        {
            key_token = strtok(NULL, " \t");
            if (key_token && strtok(NULL, " \t") == NULL)
            { // No extra arguments
                zget_command(key_token);
            }
            else
            {
                printf("Usage: zget <key>");
            }
        }
        else if (strcmp(command_token, "zrm") == 0)
        {
            key_token = strtok(NULL, " \t");
            if (key_token && strtok(NULL, " \t") == NULL)
            { // No extra arguments
                zrm_command(key_token);
            }
            else
            {
                printf("Usage: zrm <key>");
            }
        }
        else if (strcmp(command_token, "zall") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            { // No extra arguments
                zall_command();
            }
            else
            {
                printf("Usage: zall");
            }
        }
        else if (strcmp(command_token, "exit") == 0 || strcmp(command_token, "quit") == 0)
        {
            exec_time = end_timer(); // Stop timer for 'exit'
            printf("Exiting. (%.3f ms)\n", exec_time);
            save_all_data_at_exit(); // Save data before exit
            break;
        }
        else if (strcmp(command_token, "help") == 0)
        {
            printf("Available commands:\n");
            printf("  zset <key> <value> - Set a key-value pair\n");
            printf("  zget <key>         - Get value for a key\n");
            printf("  zrm <key>          - Remove a key\n");
            printf("  zall               - List all key-value pairs\n");
            printf("  exit/quit          - Exit the program\n");
            printf("  help               - Show this help\n");
            exec_time = end_timer();
            printf(" (%.3f ms)\n", exec_time);
            continue;
        }
        else
        {
            printf("Unknown command: '%s'. Type 'help' for available commands", command_token);
        }

        exec_time = end_timer();           // Stop timer after execution
        printf(" (%.3f ms)\n", exec_time); // Print execution time
    }

    free_data_list(&memory_cache, &cache_size, &cache_capacity);
    return 0;
}
