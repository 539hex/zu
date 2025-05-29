#ifndef CONFIG_H
#define CONFIG_H

#define FILENAME "z.u"
#define INITIAL_CAPACITY 10
#define INIT_DB_SIZE 5000001

// 'extern' declaration for the global variable.
// The definition will be in a .c file (e.g., zu_cache.c).
extern unsigned int HIT_THRESHOLD_FOR_CACHING;

#endif // CONFIG_H
