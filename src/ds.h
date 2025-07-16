#ifndef DS_H
#define DS_H

#include <stdlib.h> // For size_t

// --- Data Structures ---
typedef struct DataItem
{
    char *key;
    char *value;
    unsigned int hit_count;     // Hit count for caching
    unsigned int last_accessed; // Timestamp of last access
    struct DataItem *next;      // For chaining in hash table
} DataItem;

typedef struct
{
    unsigned int size;
    DataItem **table;
} HashTable;

// --- Hash Table Function Declarations ---
HashTable *create_hash_table(unsigned int size);
void free_hash_table(HashTable *ht);
unsigned int hash_function(const char *key, unsigned int size);
void hash_table_insert(HashTable *ht, const char *key, const char *value);
DataItem *hash_table_search(HashTable *ht, const char *key);
void hash_table_remove(HashTable *ht, const char *key);

// --- Helper Function Declarations ---
char *my_strdup(const char *s);
void free_data_item_contents(DataItem *item);
void free_data_list(DataItem **list, size_t *size, size_t *capacity);
void ensure_list_capacity(DataItem **list, size_t *capacity, size_t needed_size);

#endif // DS_H
