#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>

int zset_command(const char *key_to_set, const char *value_to_set, bool is_cli);
char *zget_command(const char *key_to_get, bool is_cli);
int zrm_command(const char *key);
int zall_command(void);
int init_db_command(void);
int cache_status(void);
void clear(void);

int benchmark_command(void);

#endif // COMMANDS_H
