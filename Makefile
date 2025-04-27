# Compiler and base flags
CC = gcc

# Release flags (optimized)
CFLAGS_RELEASE = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
                 -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
                 -fstrict-aliasing -ftree-vectorize -fno-stack-protector
LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

# Debug flags (for debugging with stack traces)
CFLAGS_DEBUG = -Wall -Wextra -pedantic -g -rdynamic -O0 -I./src -MMD -DDEBUG \
               -fno-omit-frame-pointer -fno-inline
LDFLAGS_DEBUG = -g -rdynamic

# Targets
TARGET = compress
DEBUG_TARGET = compress-debug
DECOMPRESS_TARGET = decompress

# Directory structure
SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = $(SRC_DIR)/debug
SECOND_PASS_DIR = $(SRC_DIR)/second_pass
WRITE_IN_FILE_DIR = $(SRC_DIR)/write_in_file
DECOMPRESS_DIR = $(SRC_DIR)/decompress

# Source files
MAIN_SRCS = $(wildcard $(SRC_DIR)/*.c)
SECOND_PASS_SRCS = $(wildcard $(SECOND_PASS_DIR)/*.c)
DEBUG_SRCS = $(wildcard $(DEBUG_DIR)/*.c)
WRITE_IN_FILE_SRCS = $(wildcard $(WRITE_IN_FILE_DIR)/*.c)
DECOMPRESS_SRCS = $(DECOMPRESS_DIR)/decompress.c

# Object files (separate release/debug builds)
RELEASE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/release/%.o,$(MAIN_SRCS)) \
               $(patsubst $(SECOND_PASS_DIR)/%.c,$(BUILD_DIR)/release/second_pass/%.o,$(SECOND_PASS_SRCS)) \
               $(patsubst $(WRITE_IN_FILE_DIR)/%.c,$(BUILD_DIR)/release/write_in_file/%.o,$(WRITE_IN_FILE_SRCS))

DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/debug/%.o,$(MAIN_SRCS)) \
             $(patsubst $(SECOND_PASS_DIR)/%.c,$(BUILD_DIR)/debug/second_pass/%.o,$(SECOND_PASS_SRCS)) \
             $(patsubst $(WRITE_IN_FILE_DIR)/%.c,$(BUILD_DIR)/debug/write_in_file/%.o,$(WRITE_IN_FILE_SRCS)) \
             $(patsubst $(DEBUG_DIR)/%.c,$(BUILD_DIR)/debug/debug/%.o,$(DEBUG_SRCS))

DECOMPRESS_OBJ = $(BUILD_DIR)/release/decompress/decompress.o

# Dependencies
DEPS = $(RELEASE_OBJS:.o=.d) $(DEBUG_OBJS:.o=.d) $(DECOMPRESS_OBJ:.o=.d)

# [Previous sections remain the same until the targets]

.PHONY: all clean release debug compress decompress help

all: release

release: $(TARGET)
	@echo "Built release version: ./$(TARGET)"

debug: $(DEBUG_TARGET)
	@echo "Built debug version: ./$(DEBUG_TARGET)"


compress: $(TARGET)
	@echo "Built compression tool: ./$(TARGET)"
	@echo "Usage: ./$(TARGET) <input_file>"

decompress: $(DECOMPRESS_TARGET)
	@echo "Built decompression tool: ./$(DECOMPRESS_TARGET)"
	@echo "Usage: ./$(DECOMPRESS_TARGET) <compressed_input> <decompressed_output>"

$(TARGET): CFLAGS = $(CFLAGS_RELEASE)
$(TARGET): LDFLAGS = $(LDFLAGS_RELEASE)
$(TARGET): $(RELEASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DEBUG_TARGET): CFLAGS = $(CFLAGS_DEBUG)
$(DEBUG_TARGET): LDFLAGS = $(LDFLAGS_DEBUG)
$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(DECOMPRESS_TARGET): $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS_RELEASE) -o $@ $^

# Release build rules
$(BUILD_DIR)/release/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/release/second_pass/%.o: $(SECOND_PASS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/release/write_in_file/%.o: $(WRITE_IN_FILE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/release/decompress/%.o: $(DECOMPRESS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

# Debug build rules
$(BUILD_DIR)/debug/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILD_DIR)/debug/second_pass/%.o: $(SECOND_PASS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILD_DIR)/debug/write_in_file/%.o: $(WRITE_IN_FILE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILD_DIR)/debug/debug/%.o: $(DEBUG_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(TARGET) $(DEBUG_TARGET) $(DECOMPRESS_TARGET) src/combined_code.c
	@echo "Cleaned all build artifacts"

-include $(DEPS)

help:
	@echo "Available targets:"
	@echo "  all         - Build release version (default)"
	@echo "  release     - Build optimized release version"
	@echo "  debug       - Build debug version with stack traces"
	@echo "  compress    - Build compression tool (release)"
	@echo "  decompress  - Build decompression tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  help        - Show this help message"
