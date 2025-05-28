#ifndef IO_H
#define IO_H

#include "ds.h"     // For DataItem
#include <stddef.h> // For size_t
#include <stdio.h>  // For FILE

// --- Disk I/O Function Declarations ---
int load_all_data_from_disk(DataItem **full_data_list, size_t *list_size, size_t *list_capacity);
void save_all_data_to_disk(DataItem *data_list, size_t list_size);
int print_all_data_from_disk(void); // New function to print directly from disk

// New optimized functions
int find_key_on_disk(const char *key, char **value, unsigned int *hit_count);
int update_key_on_disk(const char *key, const char *new_value, unsigned int new_hit_count);
int remove_key_from_disk(const char *key);
int append_key_to_disk(const char *key, const char *value, unsigned int hit_count);

// Helper function declarations
int write_item_to_file(FILE *file, const char *key, const char *value, unsigned int hit_count);

// Function to reset hit counts in the data file
void reset_hit_counts_in_file(void);

#endif // ZU_IO_H
