# Compiler and base flags
CC = gcc

BUILD_DIR = build

# Release flags (optimized)
CFLAGS_RELEASE = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
                 -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
                 -fstrict-aliasing -ftree-vectorize -fno-stack-protector
LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

# Debug flags
CFLAGS_DEBUG = -Wall -Wextra -pedantic -g -rdynamic -O0 -I./src -MMD -DDEBUG \
               -fno-omit-frame-pointer -fno-inline
LDFLAGS_DEBUG = -g -rdynamic

# Targets
COMPRESS_TARGET = compress
DECOMPRESS_TARGET = decompress
DEBUG_COMPRESS_TARGET = compress-debug

# Load source files
include used_sources.mk

# Split sources into compression and decompression files
COMPRESS_SRCS = $(filter-out src/decompress/decompress.c, $(SRCS))
DECOMPRESS_SRCS = src/decompress/decompress.c

# Object files
COMPRESS_RELEASE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(COMPRESS_SRCS))
COMPRESS_DEBUG_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/debug/%.o,$(COMPRESS_SRCS))
DECOMPRESS_OBJ = $(BUILD_DIR)/release/decompress/decompress.o

# Dependencies
DEPS = $(COMPRESS_RELEASE_OBJS:.o=.d) $(COMPRESS_DEBUG_OBJS:.o=.d) $(DECOMPRESS_OBJ:.o=.d)

.PHONY: all clean release debug compress decompress help

all: compress decompress

release: compress decompress
	@echo "Built release versions: ./$(COMPRESS_TARGET) and ./$(DECOMPRESS_TARGET)"

debug: $(DEBUG_COMPRESS_TARGET) decompress
	@echo "Built debug version: ./$(DEBUG_COMPRESS_TARGET) and ./$(DECOMPRESS_TARGET)"

compress: $(COMPRESS_TARGET)
	@echo "Built compression tool: ./$(COMPRESS_TARGET)"

decompress: $(DECOMPRESS_TARGET)
	@echo "Built decompression tool: ./$(DECOMPRESS_TARGET)"

$(COMPRESS_TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(COMPRESS_TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(COMPRESS_TARGET): $(COMPRESS_RELEASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DEBUG_COMPRESS_TARGET): CFLAGS = $(CFLAGS_DEBUG)
$(DEBUG_COMPRESS_TARGET): LDFLAGS = $(LDFLAGS_DEBUG)
$(DEBUG_COMPRESS_TARGET): $(COMPRESS_DEBUG_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DECOMPRESS_TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(DECOMPRESS_TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(DECOMPRESS_TARGET): $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

# Build rules
$(BUILD_DIR)/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(COMPRESS_TARGET) $(DEBUG_COMPRESS_TARGET) $(DECOMPRESS_TARGET)
	@echo "Cleaned all build artifacts"

-include $(DEPS)

help:
	@echo "Available targets:"
	@echo "  all         - Build both tools (default)"
	@echo "  release     - Build optimized versions of both tools"
	@echo "  debug       - Build debug version of compressor and release decompressor"
	@echo "  compress    - Build only compression tool"
	@echo "  decompress  - Build only decompression tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  help        - Show this help message"
