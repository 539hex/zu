#ifndef CONFIG_H
#define CONFIG_H

#define INITIAL_CAPACITY 10
#define INIT_DB_SIZE 5000
#define BENCHMARK_DB_SIZE 100000 // Number of key-value pairs for benchmark
#define CACHE_SIZE 1000
#define CACHE_TTL 60
#define REST_SERVER_PORT 1337
#define BENCHMARK_MODE 1 // Set to 1 to suppress output during benchmark
extern char *FILENAME;

void set_test_mode(void);

#endif // CONFIG_H
