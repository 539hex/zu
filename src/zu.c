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

// Global for thread
pthread_t server_thread;

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
                                                 // Start cache timer

        if (strcmp(command_token, "zset") == 0)
        {
            key_token = strtok(NULL, " \t");
            value_token = strtok(NULL, ""); // The rest is the value
            if (key_token && value_token)
            {
                while (*value_token && isspace((unsigned char)*value_token))
                {
                    value_token++; // Trim leading spaces
                }
                if (*value_token)
                {
                    int result = zset_command(key_token, value_token, true);
                    if (result < 0)
                    {
                        printf("Error: Operation failed.\n");
                    }
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
                char *result = zget_command(key_token, true);
                if (result == NULL)
                {
                    printf("Error: Operation failed.\n");
                }
                else
                {
                    free(result);
                }
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
                int result = zrm_command(key_token);
                if (result < 0)
                {
                    printf("Error: Operation failed.\n");
                }
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
                int result = zall_command();
                if (result < 0)
                {
                    printf("Error: Operation failed.\n");
                }
            }
            else
            {
                printf("Usage: zall");
            }
        }
        else if (strcmp(command_token, "init_db") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            { // No extra arguments
                int result = init_db_command();
                if (result < 0)
                {
                    printf("Error: Operation failed.\n");
                }
            }
            else
            {
                printf("Usage: init_db");
            }
        }
        else if (strcmp(command_token, "cache_status") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            { // No extra arguments
                int result = cache_status();
                if (result < 0)
                {
                    printf("Error: Operation failed.\n");
                }
            }
            else
            {
                printf("Usage: cache_status");
            }
        }
        else if (strcmp(command_token, "clear") == 0)
        {
            clear();                                           // Clear the terminal screen
            exec_time = command_timer_end(&command_timer_val); // Stop timer for 'clear'
            free(line);
            continue; // Don't print time twice for 'clear'
        }
        else if (strcmp(command_token, "exit") == 0 || strcmp(command_token, "quit") == 0)
        {
            exec_time = command_timer_end(&command_timer_val); // Stop timer for 'exit'
            printf("Goodbye!\n");
            free(line);

            // Join the server thread
            printf("Shutting down REST server...\n");
            // Assuming we add a way to stop the server loop, for now just cancel
            pthread_cancel(server_thread);
            pthread_join(server_thread, NULL);
            printf("REST server shut down.\n");
            break;
        }
        else if (strcmp(command_token, "benchmark") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            {
                int result = benchmark_command();
                if (result < 0)
                {
                    printf("Error: Operation failed.\n");
                }
            }
            else
            {
                printf("Usage: benchmark");
            }
        }

        else if (strcmp(command_token, "help") == 0)
        {
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
            exec_time = command_timer_end(&command_timer_val);
            free(line);
            continue; // Don't print time twice for 'help'
        }
        else
        {
            printf("Unknown command: '%s'. Type 'help' for available commands", command_token);
        }

        exec_time = command_timer_end(&command_timer_val); // Stop timer after execution
        printf("\n(%.3fms)\n", exec_time);                 // Print execution time

        free(line); // Free the line allocated by readline
    }

    free_cache();
    // Clean up readline history
    clear_history(); // Free global cache before terminating
    return 0;
}
