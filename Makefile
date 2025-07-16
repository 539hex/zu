# Compiler settings
CC = gcc
CFLAGS = -Wall -g -O2 -Isrc -std=c11
LDFLAGS = -lreadline

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

# Test suite target
test: $(TEST_EXEC)

$(TEST_EXEC): $(COMMON_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $^ -o $(TEST_EXEC) $(LDFLAGS)

# Object file rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(EXEC) $(TEST_EXEC) $(SRC_DIR)/*.o $(TEST_DIR)/*.o

# Phony targets
.PHONY: clean test
