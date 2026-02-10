# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -Isrc -std=c11
LDFLAGS = -lreadline -pie -Wl,-z,relro,-z,now

# Source directories
SRC_DIR = src
TEST_DIR = tests

# Source files for main program
MAIN_SRC = $(wildcard $(SRC_DIR)/*.c)
TEST_SRC = $(TEST_DIR)/test.c

# Object files
MAIN_OBJ = $(MAIN_SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)

# Common object files (used by both main and test)
COMMON_OBJ = $(filter-out $(SRC_DIR)/zu.o, $(MAIN_OBJ))

# Executables
EXEC = zu
TEST_EXEC = test_suite

# Main program target
$(EXEC): $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(MAIN_OBJ) -o $(EXEC) $(LDFLAGS)
	chmod 750 $(EXEC)

# Test suite target
test: $(TEST_EXEC)

$(TEST_EXEC): $(COMMON_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $^ -o $(TEST_EXEC) $(LDFLAGS)
	chmod 750 $(TEST_EXEC)

# Object file rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(EXEC) $(TEST_EXEC)
	find $(SRC_DIR) $(TEST_DIR) -name '*.o' -type f -delete 2>/dev/null || true

# Phony targets
.PHONY: clean test
