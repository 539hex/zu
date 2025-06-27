#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h> // For fork()
#include <sys/wait.h> // For waitpid()
#include <signal.h> // For kill()

#include "version.h"
#include "timer.h"
#include "commands.h"
#include "config.h"
#include "cache.h"
#include "http_server.h"

int main(void)
{
    char *line;
    char *command_token, *key_token, *value_token;
    double exec_time;
    struct timespec command_timer_val; // For the command timer
    struct timespec cache_timer_val;   // For the cache timer
    pid_t server_pid;

    server_pid = fork();

    if (server_pid == -1) {
        perror("fork failed");
        return 1;
    } else if (server_pid == 0) {
        // Child process
        start_inhouse_rest_server();
        exit(0); // Exit child process after server starts
    } else {
        // Parent process continues to CLI
        // Optionally, you can add a small delay or check if the child is running
        // to ensure the server has a chance to start before the CLI is fully active.
    }

    // No longer calling waitpid with WNOHANG here.
    // The parent will wait for the child only when exiting.

    printf("Zu %s\n", ZU_VERSION);
    printf("Type 'help' for available commands. \n");
    // Initialize readline history
    using_history();
    init_cache();
    cache_timer_start(&cache_timer_val);

    while (1)
    {
        line = readline("> ");
        if (cache_timer_end(&cache_timer_val) > CACHE_TTL)
        {
            printf("Cache expired. Clearing cache.\n");
            free_cache();
            cache_timer_val.tv_nsec = 0;
            cache_timer_val.tv_sec = 0;
            cache_timer_start(&cache_timer_val);
        }
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
                    int result = zset_command(key_token, value_token);
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
                char *result = zget_command(key_token);
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

            // Terminate the child process (HTTP server)
            if (server_pid > 0) {
                printf("Shutting down REST server...\n");
                kill(server_pid, SIGTERM); // Send SIGTERM to the child process
                waitpid(server_pid, NULL, 0); // Wait for the child process to terminate
                printf("REST server shut down.\n");
            }
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
