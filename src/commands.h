#ifndef COMMANDS_H
#define COMMANDS_H

// --- Command Function Declarations ---
void zset_command(const char *key_to_set, const char *value_to_set);
void zget_command(const char *key_to_get);
void zrm_command(const char *key_to_remove);
void zall_command(void);
void init_db_command(void);
void cleanup_db_command(void);
void cache_status(void);

#endif // COMMANDS_H
