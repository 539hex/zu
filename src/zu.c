#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h> // For fork()
#include <sys/wait.h> // For waitpid()
#include <pthread.h> // For threading
#include <stdbool.h> // If not already included via header

#include "version.h"
#include "timer.h"
#include "commands.h"
#include "cache.h"
#include "http_server.h"
#include "config.h"

// Global for thread
pthread_t server_thread;
volatile int server_shutdown = 0;


// Command enum for switch statement
typedef enum {
    CMD_ZSET,
    CMD_ZGET,
    CMD_ZRM,
    CMD_ZALL,
    CMD_INIT_DB,
    CMD_CACHE_STATUS,
    CMD_CLEAR,
    CMD_EXIT,
    CMD_BENCHMARK,
    CMD_HELP,
    CMD_UNKNOWN
} command_type_t;

// Function to determine command type
command_type_t get_command_type(const char *command) {
    if (strcmp(command, "zset") == 0) return CMD_ZSET;
    if (strcmp(command, "zget") == 0) return CMD_ZGET;
    if (strcmp(command, "zrm") == 0) return CMD_ZRM;
    if (strcmp(command, "zall") == 0) return CMD_ZALL;
    if (strcmp(command, "init_db") == 0) return CMD_INIT_DB;
    if (strcmp(command, "cache_status") == 0) return CMD_CACHE_STATUS;
    if (strcmp(command, "clear") == 0) return CMD_CLEAR;
    if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) return CMD_EXIT;
    if (strcmp(command, "benchmark") == 0) return CMD_BENCHMARK;
    if (strcmp(command, "help") == 0) return CMD_HELP;
    return CMD_UNKNOWN;
}

// Function to handle zset command
void handle_zset(char *key_token, char *value_token) {
    if (!key_token || !value_token) {
        printf("Usage: zset <key> <value>");
        return;
    }

    // Trim leading spaces from value
    while (*value_token && isspace((unsigned char)*value_token)) {
        value_token++;
    }
    
    if (!*value_token) {
        printf("Usage: zset <key> <value>");
        return;
    }

    int result = zset_command(key_token, value_token);
    if (result == CMD_SUCCESS) {
        if (DEBUG_CLI) printf("OK\n");
    } else if (result == CMD_EMPTY) {
        printf("Error: Key or value cannot be empty.\n");
    } else {
        printf("Error: Operation failed.\n");
    }
}

// Function to handle zget command
void handle_zget(char *key_token) {
    if (!key_token) {
        printf("Usage: zget <key>");
        return;
    }

    char *result_value;
    int result = zget_command(key_token, &result_value);
    if (result == CMD_SUCCESS && result_value) {
        if (DEBUG_CLI) printf("%s\n", result_value);
        free(result_value);
    } else if (result == CMD_EMPTY) {
        printf("Error: Key cannot be empty.\n");
    } else if (result == CMD_NOT_FOUND) {
        if (DEBUG_CLI) printf("Key '%s' not found.\n", key_token);
    } else {
        if (DEBUG_CLI) printf("Error: Could not access data.\n");
    }
}

// Function to handle zrm command
void handle_zrm(char *key_token) {
    if (!key_token) {
        printf("Usage: zrm <key>");
        return;
    }

    int result = zrm_command(key_token);
    if (result == CMD_SUCCESS) {
        printf("OK: Key '%s' removed.\n", key_token);
    } else if (result == CMD_NOT_FOUND) {
        printf("Key '%s' not found.\n", key_token);
    } else {
        printf("Error: Could not remove key (file error).\n");
    }
}

// Function to handle zall command
void handle_zall() {
    int result = zall_command();
    if (result == CMD_ERROR) {
        printf("(empty or file error)\n");
    }
}

// Function to handle init_db command
void handle_init_db() {
    printf("Initializing database with %d random key-value pairs...\n", INIT_DB_SIZE);
    int result = init_db_command();
    if (result == CMD_SUCCESS) {
        printf("Database initialization complete.\n");
    } else {
        printf("Error: Could not open database file for writing.\n");
    }
}

// Function to handle cache_status command
void handle_cache_status() {
    pthread_mutex_lock(&cache_mutex);
    int result = cache_status();
    if (result == CMD_ERROR) {
        printf("Cache is not initialized\n");
    } else if (result == CMD_SUCCESS && memory_cache != NULL) {
        printf("Cache status: %d items used\n", memory_cache->size);
        for (unsigned int i = 0; i < memory_cache->size; i++) {
            DataItem *item = memory_cache->table[i];
            while (item) {
                printf("  Key: %s, Value: %s, Hits: %u, Last accessed: %u\n",
                       item->key, item->value, item->hit_count, item->last_accessed);
                item = item->next;
            }
        }
    } else {
        printf("Cache is not available\n");
    }
    pthread_mutex_unlock(&cache_mutex);
}

// Function to handle benchmark command
void handle_benchmark() {
    printf("Starting benchmark with %d key-value pairs...\n", BENCHMARK_DB_SIZE);
    int result = benchmark_command();
    if (result == CMD_SUCCESS) {
        printf("Benchmark completed successfully.\n");
    } else {
        printf("Error: Benchmark failed.\n");
    }
}

// Function to handle help command
void handle_help() {
    printf("\n");
    printf("Available commands:\n");
    printf("\n");
    printf("  zset <key> <value> - Set a key-value pair\n");
    printf("  zget <key>         - Get value for a key\n");
    printf("  benchmark          - Run performance benchmark\n");
    printf("  zrm <key>          - Remove a key\n");
    printf("  zall               - List all key-value pairs\n");
    printf("  init_db            - Init DB with random key-value pairs\n");
    printf("  cache_status       - Show cache status\n");
    printf("\n");
    printf("  clear              - Clear the terminal screen\n");
    printf("  exit/quit          - Exit the program\n");
    printf("  help               - Show this help\n");
}

int main(void)
{
    char *line;
    char *command_token, *key_token, *value_token;
    double exec_time;
    struct timespec command_timer_val; // For the command timer
    struct timespec cache_timer_val;   // For the cache timer

    // Remove fork, start server in thread
    if (pthread_create(&server_thread, NULL, (void*)start_inhouse_rest_server, NULL) != 0) {
        perror("Failed to start REST server thread");
        // Continue or exit?
    }

    // No longer calling waitpid with WNOHANG here.
    // The parent will wait for the child only when exiting.

    printf("Zu %s\n", ZU_VERSION);
    printf("Type 'help' for available commands. \n");
    fflush(stdout);

    // Initialize readline history
    using_history();
    init_cache();
    cache_timer_start(&cache_timer_val);

    while (1)
    {
        line = readline("> ");
        if (line == NULL)
        {
            if (feof(stdin))
            {
                printf("\nExiting.\n");
            }
            else
            {
                perror("readline error");
            }
            break;
        }

        // Add non-empty lines to history
        if (*line)
        {
            add_history(line);
        }

        command_token = strtok(line, " \t");
        if (command_token == NULL || *command_token == '\0')
        {
            free(line);
            continue;
        }

        command_timer_start(&command_timer_val); // Start command timer

        // Use switch statement for command handling
        command_type_t cmd_type = get_command_type(command_token);

        switch (cmd_type) {
            
            case CMD_ZSET:
                key_token = strtok(NULL, " \t");
                value_token = strtok(NULL, ""); // The rest is the value
                handle_zset(key_token, value_token);
                break;

            case CMD_ZGET:
                key_token = strtok(NULL, " \t");
                if (strtok(NULL, " \t") == NULL) { // No extra arguments
                    handle_zget(key_token);
                } else {
                    printf("Usage: zget <key>");
                }
                break;

            case CMD_ZRM:
                key_token = strtok(NULL, " \t");
                if (strtok(NULL, " \t") == NULL) { // No extra arguments
                    handle_zrm(key_token);
                } else {
                    printf("Usage: zrm <key>");
                }
                break;

            case CMD_ZALL:
                if (strtok(NULL, " \t") == NULL) { // No extra arguments
                    handle_zall();
                } else {
                    printf("Usage: zall");
                }
                break;

            case CMD_INIT_DB:
                if (strtok(NULL, " \t") == NULL) { // No extra arguments
                    handle_init_db();
                } else {
                    printf("Usage: init_db");
                }
                break;

            case CMD_CACHE_STATUS:
                if (strtok(NULL, " \t") == NULL) { // No extra arguments
                    handle_cache_status();
                } else {
                    printf("Usage: cache_status");
                }
                break;

            case CMD_CLEAR:
                clear();                                           // Clear the terminal screen
                exec_time = command_timer_end(&command_timer_val); // Stop timer for 'clear'
                free(line);
                continue; // Don't print time twice for 'clear'

            case CMD_EXIT:
                exec_time = command_timer_end(&command_timer_val); // Stop timer for 'exit'
                printf("Goodbye!\n");
                free(line);

                // Join the server thread
                printf("Shutting down REST server...\n");
                // Signal the server thread to stop cleanly
                extern volatile int server_running;
                server_running = 0;

                // Wait for the server thread to finish (max 2 seconds)
                // The non-blocking socket will allow the thread to exit quickly
                for (int i = 0; i < 20; i++) { // 20 * 100ms = 2 seconds
                    usleep(100000); // 100ms
                }

                // Final join (this will wait if thread is still running)
                if (pthread_join(server_thread, NULL) != 0) {
                    perror("Failed to join server thread");
                }
                printf("REST server shut down.\n");
                goto cleanup;

            case CMD_BENCHMARK:
                if (strtok(NULL, " \t") == NULL) {
                    handle_benchmark();
                } else {
                    printf("Usage: benchmark");
                }
                break;

            case CMD_HELP:
                handle_help();
                exec_time = command_timer_end(&command_timer_val);
                free(line);
                continue; // Don't print time twice for 'help'

            case CMD_UNKNOWN:
            default:
                printf("Unknown command: '%s'. Type 'help' for available commands\n", command_token ? command_token : "(null)");
                break;
        }

        exec_time = command_timer_end(&command_timer_val); // Stop timer after execution

        printf("\n(%.3fms)\n", exec_time);                 // Print execution time

        free(line); // Free the line allocated by readline
    }

cleanup:
    free_cache();
    // Clean up readline history
    clear_history(); // Free global cache before terminating
    return 0;
}
