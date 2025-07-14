#include "http_server.h"
#include "commands.h"
#include "cache.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h> // For isxdigit
#include <sys/time.h> // For usleep

#include "config.h"
#define PORT REST_SERVER_PORT
#define BUFFER_SIZE HTTP_BUFFER_SIZE

// Add global
volatile int server_running = 1;

// Add function
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static char* url_decode(const char* str) {
    char* decoded = malloc(strlen(str) + 1);
    char* ptr = decoded;
    while (*str) {
        if (*str == '%' && isxdigit(str[1]) && isxdigit(str[2])) {
            char byte = (hex_to_int(str[1]) << 4) | hex_to_int(str[2]);
            *ptr++ = byte;
            str += 3;
        } else {
            *ptr++ = *str++;
        }
    }
    *ptr = '\0';
    return decoded;
}

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
                char* decoded = url_decode(key_buf);
                strcpy(key_buf, decoded);
                free(decoded);
            }
            else if (strcmp(param_name, "value") == 0)
            {
                strcpy(value_buf, param_value);
                char* decoded = url_decode(value_buf);
                strcpy(value_buf, decoded);
                free(decoded);
            }
        }
    }
}

// Function to parse JSON payload for POST requests - returns dynamically allocated strings
static int parse_json_payload(const char *payload, char **key, char **value) {
    *key = NULL;
    *value = NULL;
    
    // Skip leading whitespace
    while (*payload == ' ' || *payload == '\t' || *payload == '\r' || *payload == '\n') {
        payload++;
    }
    
    // Simple JSON parsing for {"key":"...", "value":"..."} with whitespace tolerance
    char *key_start = strstr(payload, "\"key\"");
    char *value_start = strstr(payload, "\"value\"");
    
    if (key_start) {
        // Find the colon after "key"
        char *colon = strchr(key_start, ':');
        if (colon) {
            // Skip whitespace after colon
            colon++;
            while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
                colon++;
            }
            // Expect opening quote
            if (*colon == '"') {
                colon++; // Skip opening quote
                char *key_end = strchr(colon, '"');
                if (key_end) {
                    int key_len = key_end - colon;
                    *key = malloc(key_len + 1);
                    if (*key) {
                        strncpy(*key, colon, key_len);
                        (*key)[key_len] = '\0';
                    }
                }
            }
        }
    }
    
    if (value_start) {
        // Find the colon after "value"
        char *colon = strchr(value_start, ':');
        if (colon) {
            // Skip whitespace after colon
            colon++;
            while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
                colon++;
            }
            // Expect opening quote
            if (*colon == '"') {
                colon++; // Skip opening quote
                char *value_end = strchr(colon, '"');
                if (value_end) {
                    int value_len = value_end - colon;
                    *value = malloc(value_len + 1);
                    if (*value) {
                        strncpy(*value, colon, value_len);
                        (*value)[value_len] = '\0';
                    }
                }
            }
        }
    }
    
    return (*key != NULL && *value != NULL) ? 1 : 0;
}

// Function to send HTTP response
static void send_response(int client_socket, int status_code, const char *status_text, const char *body) {
    // Use smaller buffer for response headers (responses are much smaller than requests)
    char response[4096];
    snprintf(response, sizeof(response), 
             "HTTP/1.1 %d %s\r\n"
             "Server: Zu/%s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s", 
             status_code, status_text, ZU_VERSION, strlen(body), body);
    send(client_socket, response, strlen(response), 0);
}

// Function to read full HTTP request including body
static int read_full_request(int client_socket, char *buffer, int buffer_size) {
    int total_read = 0;
    int bytes_read;
    
    // Read the complete request in chunks
    while (total_read < buffer_size - 1) {
        bytes_read = read(client_socket, buffer + total_read, buffer_size - 1 - total_read);
        
        if (bytes_read <= 0) {
            break;
        }
        
        total_read += bytes_read;
        buffer[total_read] = '\0';
        
        // Check if we have received the complete request
        // Look for double newline which indicates end of headers
        if (strstr(buffer, "\r\n\r\n") || strstr(buffer, "\n\n")) {
            // For POST requests, check if we have the complete body
            char *content_length_header = strstr(buffer, "Content-Length:");
            if (content_length_header) {
                int content_length = 0;
                sscanf(content_length_header, "Content-Length: %d", &content_length);
                
                // Check if body size exceeds buffer capacity
                if (content_length > (BUFFER_SIZE - 1000)) { // Reserve space for headers
                    return -1; // Request too large
                }
                
                // Find where headers end
                char *body_start = strstr(buffer, "\r\n\r\n");
                if (!body_start) {
                    body_start = strstr(buffer, "\n\n");
                }
                
                if (body_start) {
                    int separator_length = (strstr(buffer, "\r\n\r\n") == body_start) ? 4 : 2;
                    int headers_length = (body_start - buffer) + separator_length;
                    int body_read = total_read - headers_length;
                    
                    // If we have the complete body, we're done
                    if (body_read >= content_length) {
                        break;
                    }
                }
            } else {
                // No Content-Length header, assume request is complete
                break;
            }
        }
        
        // Small delay to avoid busy waiting
        usleep(1000); // 1ms
    }
    return total_read;
}

// Function to handle client requests
void handle_client(int client_socket)
{
    // Use dynamic allocation for large buffer to avoid stack overflow
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        send_response(client_socket, 500, "Internal Server Error", "{\"error\":\"Memory allocation failed\"}");
        close(client_socket);
        return;
    }
    memset(buffer, 0, BUFFER_SIZE);
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    int bytes_read = read_full_request(client_socket, buffer, BUFFER_SIZE);
    
    if (bytes_read <= 0) {
        #if DEBUG_HTTP
        printf("DEBUG: Failed to read request, bytes_read: %d\n", bytes_read);
        #endif
        free(buffer);
        close(client_socket);
        return;
    }
    
    // Check if request was too large
    if (bytes_read == -1) {
        send_response(client_socket, 413, "Payload Too Large", "{\"error\":\"Request body exceeds maximum size limit\"}");
        free(buffer);
        close(client_socket);
        return;
    }

    // Make a copy of the buffer for parsing headers (strtok modifies the string)
    char *header_buffer = malloc(BUFFER_SIZE);
    if (!header_buffer) {
        send_response(client_socket, 500, "Internal Server Error", "{\"error\":\"Memory allocation failed\"}");
        free(buffer);
        close(client_socket);
        return;
    }
    strcpy(header_buffer, buffer);
    
    // Parse HTTP request
    char *method = strtok(header_buffer, " ");
    char *uri = strtok(NULL, " ");
    
    if (!method || !uri) {
        send_response(client_socket, 400, "Bad Request", "{\"error\":\"Invalid request\"}");
        close(client_socket);
        return;
    }

    char *path = strtok(uri, "?");
    char *query = strtok(NULL, "");
    
    // Determine request type
    enum {
        REQ_GET,
        REQ_POST,
        REQ_UNKNOWN
    } request_type = REQ_UNKNOWN;
    
    if (strcmp(method, "GET") == 0) {
        request_type = REQ_GET;
    } else if (strcmp(method, "POST") == 0) {
        request_type = REQ_POST;
    }
    
    // Handle different endpoints using switch-like approach
    enum {
        ENDPOINT_HEALTH,
        ENDPOINT_GET,
        ENDPOINT_SET,
        ENDPOINT_UNKNOWN
    } endpoint = ENDPOINT_UNKNOWN;
    
    if (strcmp(path, "/health") == 0) endpoint = ENDPOINT_HEALTH;
    else if (strcmp(path, "/get") == 0) endpoint = ENDPOINT_GET;
    else if (strcmp(path, "/set") == 0) endpoint = ENDPOINT_SET;
    
    switch (endpoint) {
        case ENDPOINT_HEALTH:
            send_response(client_socket, 200, "OK", "{\"status\":\"healthy\"}");
            break;
            
        case ENDPOINT_GET:
            if (request_type != REQ_GET) {
                send_response(client_socket, 405, "Method Not Allowed", "{\"error\":\"GET method required\"}");
            } else {
                char key[1024] = {0};
                char dummy_value[1024] = {0};
                
                if (query) {
                    parse_query_params(query, key, dummy_value);
                }
                
                if (strlen(key) == 0) {
                    send_response(client_socket, 400, "Bad Request", "{\"error\":\"Missing key parameter\"}");
                } else {
                    char *result_value;
                    int result = zget_command(key, &result_value);
                    
                    if (result == CMD_SUCCESS && result_value != NULL) {
                        // Dynamically allocate response body to handle large values
                        size_t response_size = strlen(result_value) + 100; // Extra space for JSON structure
                        char *response_body = malloc(response_size);
                        if (response_body) {
                            snprintf(response_body, response_size, "{\"value\":\"%s\"}", result_value);
                            send_response(client_socket, 200, "", response_body);
                            free(response_body);
                        } else {
                            send_response(client_socket, 500, "Internal Server Error", "{\"error\":\"Memory allocation failed\"}");
                        }
                        free(result_value);
                    } else {
                        send_response(client_socket, 404, "Not Found", "{\"error\":\"Key not found\"}");
                    }
                }
            }
            break;
            
        case ENDPOINT_SET:
            if (request_type != REQ_POST) {
                send_response(client_socket, 405, "Method Not Allowed", "{\"error\":\"POST method required\"}");
            } else {
                #if DEBUG_HTTP
                printf("DEBUG: Processing SET request, buffer length: %d\n", (int)strlen(buffer));
                printf("DEBUG: Full buffer (first 200 chars): %.200s\n", buffer);
                #endif
                
                // Find the JSON payload in the request body
                char *payload_start = strstr(buffer, "\r\n\r\n");
                #if DEBUG_HTTP
                printf("DEBUG: Looking for \\r\\n\\r\\n separator, found at position: %ld\n", payload_start ? (payload_start - buffer) : -1);
                
                if (payload_start) {
                    // Print characters around separator
                    printf("DEBUG: Characters before separator: %c %c %c %c\n", 
                           *(payload_start - 4), *(payload_start - 3), *(payload_start - 2), *(payload_start - 1));
                    printf("DEBUG: Separator characters: %c %c %c %c\n", 
                           *payload_start, *(payload_start + 1), *(payload_start + 2), *(payload_start + 3));
                }
                #endif
                
                if (!payload_start) {
                    #if DEBUG_HTTP
                    printf("DEBUG: \\r\\n\\r\\n not found, trying \\n\\n\n");
                    #endif
                    // Try alternative line endings
                    payload_start = strstr(buffer, "\n\n");
                    #if DEBUG_HTTP
                    printf("DEBUG: Looking for \\n\\n separator, found at position: %ld\n", payload_start ? (payload_start - buffer) : -1);
                    
                    if (payload_start) {
                        // Print characters around separator
                        printf("DEBUG: Characters before separator: %c %c %c %c\n", 
                               *(payload_start - 4), *(payload_start - 3), *(payload_start - 2), *(payload_start - 1));
                        printf("DEBUG: Separator characters: %c %c\n", 
                               *payload_start, *(payload_start + 1));
                    }
                    #endif
                    
                    if (!payload_start) {
                        #if DEBUG_HTTP
                        printf("DEBUG: No body separator found at all\n");
                        #endif
                        send_response(client_socket, 400, "Bad Request", "{\"error\":\"Missing request body\"}");
                    } else {
                        #if DEBUG_HTTP
                        printf("DEBUG: Found \\n\\n separator at position %ld\n", payload_start - buffer);
                        #endif
                        payload_start += 2; // Skip \n\n
                    }
                } else {
                    #if DEBUG_HTTP
                    printf("DEBUG: Found \\r\\n\\r\\n separator at position %ld\n", payload_start - buffer);
                    #endif
                    payload_start += 4; // Skip \r\n\r\n
                }
                
                if (payload_start) {
                    // Skip any leading whitespace
                    while (*payload_start == ' ' || *payload_start == '\t' || *payload_start == '\r' || *payload_start == '\n') {
                        payload_start++;
                    }
                    
                    #if DEBUG_HTTP
                    printf("DEBUG: JSON payload after whitespace skip: '%s'\n", payload_start);
                    #endif
                    
                    char *key = NULL;
                    char *value = NULL;
                    int parse_result = parse_json_payload(payload_start, &key, &value);
                    
                    #if DEBUG_HTTP
                    printf("DEBUG: Parsed key: '%s', value: '%s'\n", key ? key : "NULL", value ? value : "NULL");
                    #endif
                    
                    if (!parse_result || !key || !value || strlen(key) == 0 || strlen(value) == 0) {
                        send_response(client_socket, 400, "Bad Request", "{\"error\":\"Missing key or value in JSON payload\"}");
                    } else {
                        int result = zset_command(key, value);
                        if (result == CMD_SUCCESS) {
                            send_response(client_socket, 201, "OK", "{\"status\":\"OK\"}");
                        } else {
                            send_response(client_socket, 500, "Internal Server Error", "{\"error\":\"Error setting key\"}");
                        }
                    }
                    
                    // Free dynamically allocated memory
                    if (key) free(key);
                    if (value) free(value);
                }
            }
            break;
            
        case ENDPOINT_UNKNOWN:
            send_response(client_socket, 404, "Not Found", "{\"error\":\"Endpoint not found\"}");
            break;
    }

    // Clean up allocated memory
    free(buffer);
    free(header_buffer);
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

    while (server_running)
    {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        handle_client(client_socket);
    }

    close(server_fd);
}