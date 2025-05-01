# compress.mk - Compression tool specific build rules

# Compiler and base flags
CC = gcc

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
DEBUG_COMPRESS_TARGET = compress-debug
BUILD_DIR = build

# Load source files
include used_sources.mk

# Filter out decompression sources
COMPRESS_SRCS = $(filter-out src/decompress/decompress.c, $(SRCS))

# Object files
COMPRESS_RELEASE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(COMPRESS_SRCS))
COMPRESS_DEBUG_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/debug/%.o,$(COMPRESS_SRCS))

# Dependencies
COMPRESS_DEPS = $(COMPRESS_RELEASE_OBJS:.o=.d) $(COMPRESS_DEBUG_OBJS:.o=.d)

.PHONY: compress debug

release: compress
	@echo "Built release version: ./$(COMPRESS_TARGET)"

debug: $(DEBUG_COMPRESS_TARGET)
	@echo "Built debug version: ./$(DEBUG_COMPRESS_TARGET)"

compress: $(COMPRESS_TARGET)
	@echo "Built compression tool: ./$(COMPRESS_TARGET)"

$(COMPRESS_TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(COMPRESS_TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(COMPRESS_TARGET): $(COMPRESS_RELEASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DEBUG_COMPRESS_TARGET): CFLAGS = $(CFLAGS_DEBUG)
$(DEBUG_COMPRESS_TARGET): LDFLAGS = $(LDFLAGS_DEBUG)
$(DEBUG_COMPRESS_TARGET): $(COMPRESS_DEBUG_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

# Build rules
$(BUILD_DIR)/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(COMPRESS_DEPS)
