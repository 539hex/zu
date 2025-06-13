#ifndef COMMANDS_H
#define COMMANDS_H

int zset_command(const char *key, const char *value);
char *zget_command(const char *key);
int zrm_command(const char *key);
int zall_command(void);
int init_db_command(void);
int cache_status(void);
void clear(void);

#endif // COMMANDS_H
