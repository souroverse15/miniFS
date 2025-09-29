# Makefile for miniFS project
CC = gcc
CFLAGS = -O2 -std=c17 -Wall -Wextra -Werror
LDFLAGS = 

# Source files
UTILS_SRC = minivsfs_utils.c
BUILDER_SRC = mkfs_builder.c
ADDER_SRC = mkfs_adder.c

# Object files
UTILS_OBJ = $(UTILS_SRC:.c=.o)
BUILDER_OBJ = $(BUILDER_SRC:.c=.o)
ADDER_OBJ = $(ADDER_SRC:.c=.o)

# Executables
BUILDER_EXE = mkfs_builder
ADDER_EXE = mkfs_adder

# Default target
all: $(BUILDER_EXE) $(ADDER_EXE)

# Build mkfs_builder
$(BUILDER_EXE): $(BUILDER_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build mkfs_adder
$(ADDER_EXE): $(ADDER_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
%.o: %.c minivsfs.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f *.o $(BUILDER_EXE) $(ADDER_EXE)

# Install executables to /usr/local/bin (requires sudo)
install: all
	sudo cp $(BUILDER_EXE) /usr/local/bin/
	sudo cp $(ADDER_EXE) /usr/local/bin/

# Uninstall executables from /usr/local/bin (requires sudo)
uninstall:
	sudo rm -f /usr/local/bin/$(BUILDER_EXE)
	sudo rm -f /usr/local/bin/$(ADDER_EXE)

# Run tests (placeholder)
test: all
	@echo "Running basic tests..."
	@echo "Creating test filesystem..."
	./$(BUILDER_EXE) --image test.img --size-kib 1024 --inodes 256
	@echo "Adding test file..."
	echo "Hello, World!" > test.txt
	./$(ADDER_EXE) --input test.img --output test_with_file.img --file test.txt
	@echo "Cleaning up test files..."
	rm -f test.img test_with_file.img test.txt

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build all executables (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install executables to /usr/local/bin"
	@echo "  uninstall - Remove executables from /usr/local/bin"
	@echo "  test      - Run basic tests"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall test help
