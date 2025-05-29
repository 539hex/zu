# Compiler and compiler flags
CC = gcc

GLOBAL_CFLAGS = -Wall -g -O2 -Isrc
ifeq ($(shell uname -s),Linux)
  CFLAGS = $(GLOBAL_CFLAGS) -std=gnu11 
else
  CFLAGS = $(GLOBAL_CFLAGS) -std=c11 
endif

# Add -lreadline for readline library
LDFLAGS = -lreadline 
# Add -lrt on Linux if clock_gettime requires it

# Define the source directory
SRC_DIR = src

# Automatically find all .c files in the SRC_DIR.
# This will produce a list like: src/zu.c src/zu_ds.c ...
C_SOURCES_WITH_PATH = $(wildcard $(SRC_DIR)/*.c)

# Get the basenames of the C source files (e.g., zu.c, zu_ds.c from src/zu.c, src/zu_ds.c)
C_SOURCES_BASENAMES = $(notdir $(C_SOURCES_WITH_PATH))

# Object files will be created in the current directory (root, where Makefile is).
# Their names are derived from C_SOURCES_BASENAMES (e.g., zu.o from zu.c).
OBJS = $(C_SOURCES_BASENAMES:.c=.o)
# Alternative using patsubst if you prefer:
# OBJS = $(patsubst %.c,%.o,$(C_SOURCES_BASENAMES))

# The name of your executable
EXEC = zu

# VPATH specifies a list of directories make should search for prerequisites
# that are not found in the current directory.
VPATH = $(SRC_DIR)

# Default target
all: $(EXEC)

# Rule to link the executable
$(EXEC): $(OBJS) src/config.h
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC) $(LDFLAGS)

# Generic pattern rule to compile .c files into .o files.
# When make needs to build an object file (e.g., zu.o),
# it will look for its corresponding .c file (zu.c).
# Due to VPATH, it will find it in $(SRC_DIR)/zu.c.
# The automatic variable '$<' will expand to the path of the prerequisite (e.g., src/zu.c).
# The automatic variable '$@' will expand to the target name (e.g., zu.o).
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Target to clean up build files
clean:
	rm -f $(OBJS) $(EXEC)

# Phony targets are targets that don't represent actual files
.PHONY: all clean
