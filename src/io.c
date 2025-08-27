#include "io.h"
#include "config.h"
#include "ds.h"
#include "cache.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/file.h> // For flock

// Define our record separator and escape sequences
#define RECORD_SEP '\x1E'  // Record Separator (RS) - separates records
#define ESCAPE_CHAR '\x1B' // Escape (ESC) - for escaping special characters
#define KEY_VALUE_SEP '\x1D' // Group Separator (GS) - separates key from value

int write_escaped_string(FILE *file, const char *str) {
    if (!str) return 0;
    
    for (const char *p = str; *p; p++) {
        // Escape special characters
        if (*p == RECORD_SEP || *p == ESCAPE_CHAR || *p == KEY_VALUE_SEP) {
            if (fputc(ESCAPE_CHAR, file) == EOF) return 0;
        }
        if (fputc(*p, file) == EOF) return 0;
    }
    return 1;
}

// Helper function to read an escaped string
char* read_escaped_string(FILE *file) {
    char *buffer = NULL;
    size_t buffer_size = 0;
    size_t buffer_pos = 0;
    int escaped = 0;
    
    while (1) {
        int c = fgetc(file);
        if (c == EOF) {
            if (buffer_pos == 0) return NULL; // Empty string
            break;
        }
        
        if (escaped) {
            // This character was escaped, add it as is
            escaped = 0;
        } else if (c == ESCAPE_CHAR) {
            // Next character is escaped
            escaped = 1;
            continue;
        } else if (c == RECORD_SEP || c == KEY_VALUE_SEP) {
            // End of string - unget the separator so the caller can read it
            ungetc(c, file);
            break;
        }
        
        // Ensure buffer has space
        if (buffer_pos >= buffer_size) {
            size_t new_size = buffer_size == 0 ? 32 : buffer_size * 2;
            char *new_buffer = realloc(buffer, new_size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            buffer_size = new_size;
        }
        
        buffer[buffer_pos++] = c;
    }
    
    // Add null terminator
    if (buffer_pos >= buffer_size) {
        char *new_buffer = realloc(buffer, buffer_pos + 1);
        if (!new_buffer) {
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;
    }
    buffer[buffer_pos] = '\0';
    
    return buffer;
}

// Helper function to write a single item to file
int write_item_to_file(FILE *file, const char *key, const char *value) {
    if (!write_escaped_string(file, key)) return 0;
    if (fputc(KEY_VALUE_SEP, file) == EOF) return 0;
    if (!write_escaped_string(file, value)) return 0;
    if (fputc(RECORD_SEP, file) == EOF) return 0;
    return 1;
}

// Helper function to read a single item from file
int read_item_from_file(FILE *file, char **key, char **value) {
    *key = read_escaped_string(file);
    if (!*key) return 0; // End of file or error
    
    int sep = fgetc(file);
    if (sep != KEY_VALUE_SEP) {
        free(*key);
        return -1; // Invalid format
    }
    
    *value = read_escaped_string(file);
    if (!*value) {
        free(*key);
        return -1; // Error reading value
    }
    
    int record_sep = fgetc(file);
    if (record_sep != RECORD_SEP) {
        free(*key);
        free(*value);
        return -1; // Invalid format
    }
    
    return 1; // Success
}

int load_all_data_from_disk(DataItem **full_data_list, size_t *list_size, size_t *list_capacity)
{
    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        return 1; // File not found, that's okay
    }
    if (flock(fileno(file), LOCK_SH) == -1) { // Shared lock for reading
        fclose(file);
        return 0;
    }

    char *current_key = NULL;
    char *current_value = NULL;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return 0;
        }

        ensure_list_capacity(full_data_list, list_capacity, *list_size + 1);
        (*full_data_list)[*list_size].key = my_strdup(current_key);
        (*full_data_list)[*list_size].value = my_strdup(current_value);

        if (!(*full_data_list)[*list_size].key || !(*full_data_list)[*list_size].value)
        {
            perror("Failed to allocate memory for data list entry");
            if ((*full_data_list)[*list_size].key) free((*full_data_list)[*list_size].key);
            if ((*full_data_list)[*list_size].value) free((*full_data_list)[*list_size].value);
            free(current_key);
            free(current_value);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return 0;
        }

        (*list_size)++;
        free(current_key);
        free(current_value);
    }

    if (ferror(file))
    {
        perror("File read error during full load");
        flock(fileno(file), LOCK_UN);
        fclose(file);
        return 0;
    }
    flock(fileno(file), LOCK_UN);
    fclose(file);
    return 1;
}

void save_all_data_to_disk(DataItem *data_list, size_t list_size)
{
    FILE *file = fopen(FILENAME, "wb");
    if (file == NULL)
    {
        perror("Failed to open file for writing all data");
        return;
    }
    if (flock(fileno(file), LOCK_EX) == -1) { // Exclusive lock for writing
        fclose(file);
        return;
    }

    for (size_t i = 0; i < list_size; i++)
    {
        if (!write_item_to_file(file, data_list[i].key, data_list[i].value))
        {
            perror("Error writing data to disk file");
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return;
        }
    }

    if (fclose(file) != 0)
    {
        perror("Failed to close file after writing all data");
    } else {
        flock(fileno(file), LOCK_UN);
    }
}

int print_all_data_from_disk(void)
{
    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        printf("(empty)\n");
        return 0; // File not found is considered empty
    }
    if (flock(fileno(file), LOCK_SH) == -1) {
        fclose(file);
        return 0;
    }

    int key_count = 0;
    char *current_key = NULL;
    char *current_value = NULL;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            printf("Error: Invalid database format\n");
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return 0;
        }

        printf("%s:%s \n", current_key, current_value);
        key_count++;

        // Free the current pair
        free(current_key);
        free(current_value);
    }

    if (ferror(file))
    {
        printf("Error: File read error during print\n");
        flock(fileno(file), LOCK_UN);
        fclose(file);
        return 0;
    }

    flock(fileno(file), LOCK_UN);
    fclose(file);
    printf("Total keys: %d\n", key_count);
    if (key_count == 0)
    {
        return 0; // Empty file
    }

    return 1;
}

int find_key_on_disk(const char *key, char **value)
{
    pthread_mutex_lock(&file_mutex);

    FILE *file = fopen(FILENAME, "rb");
    if (!file)
    {
        pthread_mutex_unlock(&file_mutex);
        return -1; // File error
    }
    if (flock(fileno(file), LOCK_SH) == -1) {
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    char *current_key = NULL;
    char *current_value = NULL;
    int found = 0;

    while (!found && read_item_from_file(file, &current_key, &current_value) > 0)
    {
        if (strcmp(current_key, key) == 0)
        {
            *value = current_value; // Transfer ownership of value to caller
            free(current_key);      // Free the key as we don't need it
            found = 1;
        }
        else
        {
            free(current_key);
            free(current_value);
        }
    }

    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return found;
}

int remove_key_from_disk(const char *key)
{
    pthread_mutex_lock(&file_mutex);

    // First read all items except the one to remove into memory
    DataItem *items = NULL;
    size_t items_size = 0;
    size_t items_capacity = 0;
    int found = 0;

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        pthread_mutex_unlock(&file_mutex);
        return 0; // File doesn't exist
    }
    if (flock(fileno(file), LOCK_EX) == -1) { // Exclusive for read-modify-write
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    char *current_key = NULL;
    char *current_value = NULL;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return -1;
        }

        if (strcmp(current_key, key) != 0)
        {
            // Add to items list if it's not the key to remove
            if (items_size >= items_capacity)
            {
                items_capacity = items_capacity == 0 ? 10 : items_capacity * 2;
                items = realloc(items, items_capacity * sizeof(DataItem));
                if (!items)
                {
                    free(current_key);
                    free(current_value);
                    flock(fileno(file), LOCK_UN);
                    fclose(file);
                    pthread_mutex_unlock(&file_mutex);
                    return -1;
                }
            }
            items[items_size].key = current_key;
            items[items_size].value = current_value;
            items_size++;
        }
        else
        {
            found = 1;
            free(current_key);
            free(current_value);
        }
    }

    flock(fileno(file), LOCK_UN);
    fclose(file);

    if (!found)
    {
        // Free any allocated items
        for (size_t i = 0; i < items_size; i++)
        {
            free(items[i].key);
            free(items[i].value);
        }
        free(items);
        pthread_mutex_unlock(&file_mutex);
        return 0; // Key not found
    }

    // Write back all items except the removed one
    file = fopen(FILENAME, "wb");
    if (file == NULL)
    {
        // Free any allocated items
        for (size_t i = 0; i < items_size; i++)
        {
            free(items[i].key);
            free(items[i].value);
        }
        free(items);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    if (flock(fileno(file), LOCK_EX) == -1) {
        // Free any allocated items
        for (size_t i = 0; i < items_size; i++)
        {
            free(items[i].key);
            free(items[i].value);
        }
        free(items);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    // Write all items back to file
    for (size_t i = 0; i < items_size; i++)
    {
        if (!write_item_to_file(file, items[i].key, items[i].value))
        {
            // Free remaining items
            for (size_t j = i; j < items_size; j++)
            {
                free(items[j].key);
                free(items[j].value);
            }
            free(items);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            return -1;
        }
        free(items[i].key);
        free(items[i].value);
    }

    free(items);
    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 1; // Successfully removed
}

int update_key_on_disk(const char *key, const char *new_value)
{
    pthread_mutex_lock(&file_mutex);

    // Read all items into memory
    DataItem *items = NULL;
    size_t items_size = 0;
    size_t items_capacity = 0;
    int found = 0;

    FILE *file = fopen(FILENAME, "rb");
    if (file != NULL)
    {
        if (flock(fileno(file), LOCK_EX) == -1) {
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            return -1;
        }

        char *current_key = NULL;
        char *current_value = NULL;

        while (read_item_from_file(file, &current_key, &current_value) > 0)
        {
            if (strcmp(current_key, key) == 0)
            {
                // Update the value
                free(current_value);
                current_value = my_strdup(new_value);
                found = 1;
            }

            if (items_size >= items_capacity)
            {
                items_capacity = items_capacity == 0 ? 10 : items_capacity * 2;
                items = realloc(items, items_capacity * sizeof(DataItem));
            }
            items[items_size].key = current_key;
            items[items_size].value = current_value;
            items_size++;
        }
        flock(fileno(file), LOCK_UN);
        fclose(file);
    }

    // If key was not found, add it
    if (!found)
    {
        if (items_size >= items_capacity)
        {
            items_capacity = items_capacity == 0 ? 10 : items_capacity * 2;
            items = realloc(items, items_capacity * sizeof(DataItem));
        }
        items[items_size].key = my_strdup(key);
        items[items_size].value = my_strdup(new_value);
        items_size++;
    }

    // Write everything back to the file
    file = fopen(FILENAME, "wb");
    if (file == NULL)
    {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    if (flock(fileno(file), LOCK_EX) == -1) {
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    for (size_t i = 0; i < items_size; i++)
    {
        write_item_to_file(file, items[i].key, items[i].value);
        free(items[i].key);
        free(items[i].value);
    }

    free(items);
    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// Helper function to clean up duplicate keys in the database
int cleanup_duplicate_keys(void)
{
    pthread_mutex_lock(&file_mutex);

    // First read all unique items into memory
    DataItem *items = NULL;
    size_t items_size = 0;
    size_t items_capacity = 0;

    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL)
    {
        pthread_mutex_unlock(&file_mutex);
        return 0; // File doesn't exist
    }
    if (flock(fileno(file), LOCK_EX) == -1) {
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    char *current_key = NULL;
    char *current_value = NULL;

    while (1)
    {
        int result = read_item_from_file(file, &current_key, &current_value);
        if (result <= 0)
        {
            if (result == 0) // End of file
                break;
            // Error occurred
            free(current_key);
            free(current_value);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            return -1;
        }

        // Check if this key already exists in our items list
        int is_duplicate = 0;
        for (size_t i = 0; i < items_size; i++)
        {
            if (strcmp(items[i].key, current_key) == 0)
            {
                is_duplicate = 1;
                break;
            }
        }

        if (!is_duplicate)
        {
            // Add to items list if it's not a duplicate
            if (items_size >= items_capacity)
            {
                items_capacity = items_capacity == 0 ? 10 : items_capacity * 2;
                items = realloc(items, items_capacity * sizeof(DataItem));
                if (!items)
                {
                    free(current_key);
                    free(current_value);
                    flock(fileno(file), LOCK_UN);
                    fclose(file);
                    pthread_mutex_unlock(&file_mutex);
                    return -1;
                }
            }
            items[items_size].key = current_key;
            items[items_size].value = current_value;
            items_size++;
        }
        else
        {
            // Free duplicate key-value pair
            free(current_key);
            free(current_value);
        }
    }

    flock(fileno(file), LOCK_UN);
    fclose(file);

    // Write back only unique items
    file = fopen(FILENAME, "wb");
    if (file == NULL)
    {
        // Free any allocated items
        for (size_t i = 0; i < items_size; i++)
        {
            free(items[i].key);
            free(items[i].value);
        }
        free(items);
        return -1;
    }
    if (flock(fileno(file), LOCK_EX) == -1) {
        // Free any allocated items
        for (size_t i = 0; i < items_size; i++)
        {
            free(items[i].key);
            free(items[i].value);
        }
        free(items);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    // Write all unique items back to file
    for (size_t i = 0; i < items_size; i++)
    {
        if (!write_item_to_file(file, items[i].key, items[i].value))
        {
            // Free remaining items
            for (size_t j = i; j < items_size; j++)
            {
                free(items[j].key);
                free(items[j].value);
            }
            free(items);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            return -1;
        }
        free(items[i].key);
        free(items[i].value);
    }

    free(items);
    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 1; // Successfully cleaned up
}

// Internal function that assumes mutex is already locked
static int find_key_on_disk_internal(const char *key, char **value)
{
    FILE *file = fopen(FILENAME, "rb");
    if (!file)
    {
        return -1; // File error
    }
    if (flock(fileno(file), LOCK_SH) == -1) {
        fclose(file);
        return -1;
    }

    char *current_key = NULL;
    char *current_value = NULL;
    int found = 0;

    while (!found && read_item_from_file(file, &current_key, &current_value) > 0)
    {
        if (strcmp(current_key, key) == 0)
        {
            *value = current_value; // Transfer ownership of value to caller
            free(current_key);      // Free the key as we don't need it
            found = 1;
        }
        else
        {
            free(current_key);
            free(current_value);
        }
    }

    flock(fileno(file), LOCK_UN);
    fclose(file);
    return found;
}

int append_key_to_disk(const char *key, const char *value)
{
    pthread_mutex_lock(&file_mutex);

    // First check if key exists (without acquiring mutex again)
    char *existing_value = NULL;
    int exists = find_key_on_disk_internal(key, &existing_value);
    if (exists > 0)
    {
        free(existing_value);
        pthread_mutex_unlock(&file_mutex);
        return 0; // Key already exists
    }

    FILE *file = fopen(FILENAME, "ab"); // Open for append
    if (file == NULL)
    {
        // If file doesn't exist, try to create it
        file = fopen(FILENAME, "wb");
        if (file == NULL) {
            pthread_mutex_unlock(&file_mutex);
            return 0;
        }
    }
    if (flock(fileno(file), LOCK_EX) == -1) {
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return 0;
    }

    int success = write_item_to_file(file, key, value);
    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return success;
}

// Helper function to check if database exists and create it if needed
int ensure_database_exists(void) {
    pthread_mutex_lock(&file_mutex);

    FILE *file = fopen(FILENAME, "rb");
    if (file) {
        flock(fileno(file), LOCK_SH);
        flock(fileno(file), LOCK_UN);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return 1;  // Database exists
    }

    // Database doesn't exist, ask user
    printf("Database does not exist. Create empty database? (YES/NO): ");
    char response[10];
    if (!fgets(response, sizeof(response), stdin)) {
        pthread_mutex_unlock(&file_mutex);
        return 0;  // Error reading input
    }

    // Remove newline if present
    response[strcspn(response, "\n")] = 0;
    
    // Convert to uppercase for comparison
    for (char *p = response; *p; p++) {
        *p = toupper(*p);
    }

    if (strcmp(response, "YES") == 0) {
        file = fopen(FILENAME, "wb");
        if (file) {
            flock(fileno(file), LOCK_EX);
            flock(fileno(file), LOCK_UN);
            fclose(file);
            printf("Empty database created.\n");
            pthread_mutex_unlock(&file_mutex);
            return 1;
        }
        printf("Error: Could not create database file.\n");
        pthread_mutex_unlock(&file_mutex);
        return 0;
    }

    printf("Database creation cancelled.\n");
    pthread_mutex_unlock(&file_mutex);
    return 0;
}