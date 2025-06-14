# Compiler and base flags
CC = gcc
STD = -std=c17  # Explicitly set C standard (C11 or C17)
BUILD_DIR = build

# Release flags (optimized)
CFLAGS_RELEASE = $(STD) -Wall -Wextra -pedantic -O3 -march=native -flto \
                 -funroll-loops -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math \
                 -fno-math-errno -fstrict-aliasing -ftree-vectorize \
                 -fno-stack-protector
LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

# Debug flags
CFLAGS_DEBUG = $(STD) -Wall -Wextra -pedantic -g -rdynamic -O0 -I./src -MMD \
               -DDEBUG -fno-omit-frame-pointer -fno-inline
LDFLAGS_DEBUG = -g -rdynamic

# Targets and dependencies
COMPRESS_TARGET = compress
DECOMPRESS_TARGET = decompress
DEBUG_COMPRESS_TARGET = compress-debug

include used_sources.mk

COMPRESS_SRCS = $(filter-out src/decompress/decompress.c, $(SRCS))
DECOMPRESS_SRCS = src/decompress/decompress.c

COMPRESS_RELEASE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(COMPRESS_SRCS))
COMPRESS_DEBUG_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/debug/%.o,$(COMPRESS_SRCS))
DECOMPRESS_OBJ = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(DECOMPRESS_SRCS))

DEPS = $(COMPRESS_RELEASE_OBJS:.o=.d) $(COMPRESS_DEBUG_OBJS:.o=.d) $(DECOMPRESS_OBJ:.o=.d)

.PHONY: all clean release debug compress decompress help

all: compress decompress

release: compress decompress
	@echo "Built release versions: ./compress and ./decompress"

debug: $(DEBUG_COMPRESS_TARGET) decompress
	@echo "Built debug version: ./compress-debug and ./decompress"

compress: $(COMPRESS_RELEASE_OBJS)
	$(CC) $(LDFLAGS_RELEASE) -o $(COMPRESS_TARGET) $^ -lm
	@echo "Built compression tool: ./compress"
	@echo "Static memory usage (compress):"
	@size $(COMPRESS_TARGET)

decompress: $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS_RELEASE) -o $(DECOMPRESS_TARGET) $^ -lm
	@echo "Built decompression tool: ./decompress"
	@echo "Static memory usage (decompress):"
	@size $(DECOMPRESS_TARGET)

$(DEBUG_COMPRESS_TARGET): $(COMPRESS_DEBUG_OBJS)
	$(CC) $(LDFLAGS_DEBUG) -o $@ $^ -lm
	@echo "Built debug compression tool: ./compress-debug"
	@echo "Static memory usage (compress-debug):"
	@size $@

# Compile rules
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
