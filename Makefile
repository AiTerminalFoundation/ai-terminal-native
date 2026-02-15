# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -Icore

# Auto-detect all .c files
SRC = $(wildcard src/*.c src/core/*.c)

# Output binary
TARGET = aiterm

# Default rule
all: $(TARGET)

# Link object files into executable
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

