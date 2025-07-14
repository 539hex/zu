#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>

// Command return codes
#define CMD_SUCCESS 0
#define CMD_ERROR -1
#define CMD_NOT_FOUND -2
#define CMD_EMPTY -3

// Function signatures - all return status codes, no printing
int zset_command(const char *key_to_set, const char *value_to_set);
int zget_command(const char *key_to_get, char **result_value);
int zrm_command(const char *key);
int zall_command(void);
int init_db_command(void);
int cache_status(void);
void clear(void);
int benchmark_command(void);

#endif // COMMANDS_H
