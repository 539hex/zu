#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "../src/commands.h"
#include "../src/config.h"
#include "../src/cache.h"
#include "../src/io.h"

/* The following lines make up our testing "framework" :) */
static int tests = 0, fails = 0, skips = 0;
#define test(_s) { printf("#%02d ", ++tests); printf(_s); }
#define test_cond(_c) do { if(_c) { printf("\033[0;32mPASSED\033[0;0m\n\n"); } else { printf("\033[0;31mFAILED\033[0;0m\n\n"); fails++; } } while(0)
#define test_skipped() { printf("\033[01;33mSKIPPED\033[0;0m\n\n"); skips++; }


// Helper function to clean up test database
static void cleanup_test_db(void) {
    unlink(FILENAME);
}

// Helper function to initialize test database
static void init_test_db(void) {
    // Create an empty database file
    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to create test database file");
        exit(1);
    }
    close(fd);
}

// Test basic set and get operations
static void test_basic_operations(void) {
    test("Basic set and get operations\n");
    cleanup_test_db();
    init_test_db();
    
    // Test setting a value
    assert(zset_command("test_key", "test_value") == 1);
    
    // Test getting the value
    char* value = NULL;
    int result = find_key_on_disk("test_key", &value);
    // Test getting non-existent key
    assert(zget_command("nonexistent_key") == NULL);
    test_cond(result > 0 && value != NULL && strcmp(value, "test_value") == 0);
    if (value) free(value);
    
    
}

// Test cache functionality
static void test_cache_operations(void) {
    test("Cache operations\n");
    cleanup_test_db();
    init_test_db();
    
    // Set a value
    assert(zset_command("cache_key", "cache_value") == 1);
    
    // First get should cache the value
    assert(strcmp(zget_command("cache_key"), "cache_value") == 0);
    
    // Verify it's in cache
    DataItem *item = get_from_cache("cache_key");
    test_cond(item != NULL && strcmp(item->value, "cache_value") == 0);
}

// Test removal operation
static void test_remove_operation(void) {
    test("Remove operation\n");
    cleanup_test_db();
    init_test_db();
    
    // Set a value
    assert(zset_command("remove_key", "remove_value") == 1);
    
    // Remove the value
    assert(zrm_command("remove_key") == 1);
    
    // Try to get removed value
    char* value = NULL;
    int result = find_key_on_disk("remove_key", &value);
    // Try to remove non-existent key
    assert(zrm_command("nonexistent_key") == 0);
    if (value) free(value);
    test_cond(result == 0 && value == NULL);
}

// Test listing all keys
static void test_list_all(void) {
    test("List all operation\n");
    cleanup_test_db();
    init_test_db();
    
    // Set multiple values
    assert(zset_command("key1", "value1") == 1);
    assert(zset_command("key2", "value2") == 1);
    assert(zset_command("key3", "value3") == 1);
    
    // Get all keys
    int result = zall_command();
    assert(result == 1);
    
    
    // Test empty database
    cleanup_test_db();
    init_test_db();
    result = zall_command();
    assert(result == -1);
    test_cond(result == -1);
}

// Test cache status
static void test_cache_status(void) {
    test("Cache status operation\n");
    cleanup_test_db();
    init_test_db();
    
    // Set and get a value to ensure it's cached
    int result = zset_command("status_key", "status_value");
    assert(result == 1);
    char* value = zget_command("status_key");
    assert(strcmp(value, "status_value") == 0);
    free(value);
    
    // Check cache status
    result = cache_status();
    assert(result == 1);
    
    // Test with uninitialized cache
    free_cache();
    assert(cache_status() == -1);
    test_cond(result);
}

// Test database initialization
static void test_db_init(void) {
    test("Database initialization\n");
    cleanup_test_db();
    init_test_db();
    int result = init_db_command();
    assert(result == 1);
    
    // Verify that some keys were created
    result = zall_command();
    assert(result == 1);
    test_cond(result == 1);
}

int main(void) {
    printf("\n=== Zu Test Suite ===\n\n");
    set_test_mode();
    init_cache();

    
    // Run all tests
    test_basic_operations();
    test_cache_operations();
    test_remove_operation();
    test_list_all();
    test_cache_status();
    test_db_init();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests);
    printf("Passed: %d\n", tests - fails - skips);
    printf("Failed: %d\n", fails);
    printf("Skipped: %d\n", skips);
    
    // Clean up
    cleanup_test_db();
    
    return fails > 0 ? 1 : 0;
} 