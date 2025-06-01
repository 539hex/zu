#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "version.h"
#include "config.h"
#include "timer.h"
#include "cache.h"
#include "commands.h"
#include "io.h"

int main(void)
{
    char *line;
    char *command_token, *key_token, *value_token;
    double exec_time;
    struct timespec timer_val; // For the timer

    printf("Zu %s\n", ZU_VERSION);
    // HIT_THRESHOLD_FOR_CACHING is extern from zu_config.h, defined in zu_cache.c
    printf("Type 'help' for available commands. \n");
    // Allocate memory for cache
    // Initialize readline history
    using_history();

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

        zu_timer_start(&timer_val); // Start timer

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
        else if (strcmp(command_token, "init_db") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            { // No extra arguments
                init_db_command();
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
                cache_status();
            }
            else
            {
                printf("Usage: cache_status");
            }
        }
        else if (strcmp(command_token, "cleanup_db") == 0)
        {
            if (strtok(NULL, " \t") == NULL)
            { // No extra arguments
                cleanup_db_command();
            }
            else
            {
                printf("Usage: cleanup_db");
            }
        }
        else if (strcmp(command_token, "exit") == 0 || strcmp(command_token, "quit") == 0)
        {
            exec_time = zu_timer_end(&timer_val); // Stop timer for 'exit'
            printf("Goodbye!\n");
            free(line);
            break;
        }
        else if (strcmp(command_token, "help") == 0)
        {
            printf("Available commands:\n");
            printf("  zset <key> <value> - Set a key-value pair\n");
            printf("  zget <key>         - Get value for a key\n");
            printf("  zrm <key>          - Remove a key\n");
            printf("  zall               - List all key-value pairs\n");
            printf("  init_db            - Init DB with random key-value pairs\n");
            printf("  cleanup_db         - Remove duplicate keys from database\n");
            printf("  cache_status       - Show cache status\n");
            printf("  exit/quit          - Exit the program\n");
            printf("  help               - Show this help\n");
            exec_time = zu_timer_end(&timer_val);
            printf(" (%.3f ms)\n", exec_time); // Print time for help as well
            free(line);
            continue; // Don't print time twice for 'help'
        }
        else
        {
            printf("Unknown command: '%s'. Type 'help' for available commands", command_token);
        }

        exec_time = zu_timer_end(&timer_val); // Stop timer after execution
        printf("\n(%.3fms)\n", exec_time);    // Print execution time

        free(line); // Free the line allocated by readline
    }

    // Clean up readline history
    clear_history(); // Free global cache before terminating
    return 0;
}
