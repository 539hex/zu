#ifndef IO_BENCHMARK_H
#define IO_BENCHMARK_H

#include <stdio.h>

// Function to initialize a temporary database for benchmarking
int init_benchmark_db(const char *filename, int num_entries);

// Function to find a key in the benchmark database
char *find_key_in_benchmark_db(const char *filename, const char *key);

// Function to get all keys from the benchmark database
char **get_all_keys_from_benchmark_db(const char *filename, int *num_keys);

// Function to clean up the benchmark database
void cleanup_benchmark_db(const char *filename);

#endif // IO_BENCHMARK_H