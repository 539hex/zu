#include "http_server.h"
#include "commands.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "config.h"
#define PORT REST_SERVER_PORT
#define BUFFER_SIZE 1024

// Function to parse URL-encoded key-value pairs
static void parse_query_params(char *query, char *key_buf, char *value_buf)
{
    char *token;
    char *rest = query; // Use a temporary pointer for strtok_r

    // Initialize buffers to empty strings
    key_buf[0] = '\0';
    value_buf[0] = '\0';

    // Iterate through parameters separated by '&'
    while ((token = strtok_r(rest, "&", &rest)))
    {
        char *equals = strchr(token, '=');
        if (equals)
        {
            *equals = '\0'; // Null-terminate the parameter name
            char *param_name = token;
            char *param_value = equals + 1;

            if (strcmp(param_name, "key") == 0)
            {
                strcpy(key_buf, param_value);
            }
            else if (strcmp(param_name, "value") == 0)
            {
                strcpy(value_buf, param_value);
            }
        }
    }
}

// Function to handle client requests
void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    read(client_socket, buffer, BUFFER_SIZE);

    // Simple HTTP request parsing
    char *method = strtok(buffer, " ");
    char *uri = strtok(NULL, " ");

    if (method && uri)
    {
        char *path = strtok(uri, "?");
        char *query = strtok(NULL, "");

        if (strcmp(path, "/set") == 0)
        {
            char key[256] = {0}, value[256] = {0};
            if (query)
            {
                parse_query_params(query, key, value);
            }

            if (strlen(key) > 0 && strlen(value) > 0)
            {
                int result = zset_command(key, value);
                if (result == 1)
                {
                    char *response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"OK\"}";
                    send(client_socket, response, strlen(response), 0);
                }
                else
                {
                    char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\n\r\n{\"status\":\"Error setting key\"}";
                    send(client_socket, response, strlen(response), 0);
                }
            }
            else
            {
                char *response = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{\"status\":\"Missing key or value\"}";
                send(client_socket, response, strlen(response), 0);
            }
        }
        else if (strcmp(path, "/get") == 0)
        {
            char key[256] = {0}, dummy_value[256] = {0}; // dummy_value to match parse_query_params signature
            if (query)
            {
                parse_query_params(query, key, dummy_value);
            }

            if (strlen(key) > 0)
            {
                char *result = zget_command(key);
                if (result != NULL)
                {
                    char response[BUFFER_SIZE];
                    snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"OK\", \"value\":\"%s\"}", result);
                    send(client_socket, response, strlen(response), 0);
                    free(result); // zget_command returns a malloc'd string
                }
                else
                {
                    char *response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n\r\n{\"status\":\"Key not found\"}";
                    send(client_socket, response, strlen(response), 0);
                }
            }
        }
        else if (strcmp(path, "/health") == 0)
        {
            char *response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"healthy\"}";
            send(client_socket, response, strlen(response), 0);
        }
        else
        {
            char *response = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{\"status\":\"Missing key\"}";
            send(client_socket, response, strlen(response), 0);
        }
    }
    else
    {
        char *response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n\r\n{\"status\":\"Not Found\"}";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
}

void start_inhouse_rest_server(void)
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    init_cache(); // Ensure cache is initialized for zget/zset

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 1337
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Starting in-house REST server on port %d\n", PORT);

    while (1)
    {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        handle_client(client_socket);
    }
}