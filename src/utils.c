#include <stdlib.h>
#include <string.h>
#include <stdio.h>


void generate_random_alphanumeric(char *str, size_t length) {
    // Alphanumeric character set: a-z, A-Z, 0-9
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1; // Exclude null terminator

    if (str == NULL || length == 0) {
        if (str && length == 0) str[0] = '\0'; // Handle zero length case
        return;
    }

    for (size_t i = 0; i < length; i++) {
        int key = rand() % charset_size; // Generate a random index into the charset
        str[i] = charset[key];
    }
    str[length] = '\0'; // Null-terminate the string
}
