# decompress.mk - Decompression tool specific build rules

# Compiler and base flags
CC = gcc

# Release flags (optimized)
CFLAGS_RELEASE = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
                 -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
                 -fstrict-aliasing -ftree-vectorize -fno-stack-protector
LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

# Targets
DECOMPRESS_TARGET = decompress
BUILD_DIR = build

# Load source files
include used_sources.mk

# Only decompression source
DECOMPRESS_SRCS = src/decompress/decompress.c

# Object files
DECOMPRESS_OBJ = $(BUILD_DIR)/release/decompress/decompress.o

# Dependencies
DECOMPRESS_DEPS = $(DECOMPRESS_OBJ:.o=.d)

.PHONY: decompress

decompress: $(DECOMPRESS_TARGET)
	@echo "Built decompression tool: ./$(DECOMPRESS_TARGET)"

$(DECOMPRESS_TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(DECOMPRESS_TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(DECOMPRESS_TARGET): $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

# Build rule
$(BUILD_DIR)/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DECOMPRESS_DEPS)
