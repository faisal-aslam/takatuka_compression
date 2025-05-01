# Compiler and flags
CC = gcc

# Compiler flags
CFLAGS_RELEASE = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
                 -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
                 -fstrict-aliasing -ftree-vectorize -fno-stack-protector

LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

CFLAGS_DEBUG = -Wall -Wextra -pedantic -g -rdynamic -O0 -I./src -MMD -DDEBUG \
               -fno-omit-frame-pointer -fno-inline

LDFLAGS_DEBUG = -g -rdynamic

# Targets
TARGET = compress
DEBUG_TARGET = compress-debug
DECOMPRESS_TARGET = decompress

# Load source files from external file
include used_sources.mk

# Object files
RELEASE_OBJS = $(patsubst src/%.c,build/release/%.o,$(SRCS))
DEBUG_OBJS = $(patsubst src/%.c,build/debug/%.o,$(SRCS))

# Dependencies
DEPS = $(RELEASE_OBJS:.o=.d) $(DEBUG_OBJS:.o=.d)

.PHONY: all release debug compress decompress clean help

all: release

release: $(TARGET)
	@echo "Built release version: ./$(TARGET)"

debug: $(DEBUG_TARGET)
	@echo "Built debug version: ./$(DEBUG_TARGET)"

compress: $(TARGET)

decompress: $(DECOMPRESS_TARGET)

$(TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(TARGET): $(RELEASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DEBUG_TARGET): CFLAGS = $(CFLAGS_DEBUG)
$(DEBUG_TARGET): LDFLAGS = $(LDFLAGS_DEBUG)
$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

# Decompress target (only 1 source file assumed)
$(DECOMPRESS_TARGET): build/release/decompress/decompress.o
	$(CC) $(LDFLAGS_RELEASE) -o $@ $^

# Build rules
build/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

build/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

build/release/decompress/%.o: src/decompress/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

clean:
	@rm -rf build $(TARGET) $(DEBUG_TARGET) $(DECOMPRESS_TARGET)
	@echo "Cleaned all build artifacts"

-include $(DEPS)

help:
	@echo "Available targets:"
	@echo "  all         - Build release version (default)"
	@echo "  release     - Build optimized release version"
	@echo "  debug       - Build debug version"
	@echo "  compress    - Alias for release target"
	@echo "  decompress  - Build decompression tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  help        - Show this help message"
