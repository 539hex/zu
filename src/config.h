#ifndef CONFIG_H
#define CONFIG_H

#define INITIAL_CAPACITY 10
#define INIT_DB_SIZE 50
#define BENCHMARK_DB_SIZE 100000 // Number of key-value pairs for benchmark
#define CACHE_SIZE 1000
#define CACHE_TTL 60
#define REST_SERVER_PORT 1337
#define HTTP_BUFFER_SIZE 1048576 // 1MB
#define DEBUG_CLI 1 // Set to 1 to enable CLI output, 0 to disable
#define DEBUG_HTTP 0 // Set to 1 to enable HTTP server output, 0 to disable
extern char *FILENAME;

void set_test_mode(void);

#endif // CONFIG_H
