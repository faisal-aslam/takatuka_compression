# ===== Configuration =====
CC := gcc
STD := -std=c17
BUILD_DIR := build

# ===== Directory Structure =====
SRC_DIR := src
INC_DIRS := \
    $(SRC_DIR) \
    $(SRC_DIR)/map \
    $(SRC_DIR)/graph \
    $(SRC_DIR)/files \
	$(SRC_DIR)/files/compression/ \
	$(SRC_DIR)/files/decompression/ \
    $(SRC_DIR)/decompress

# ===== Compiler Flags =====
WARNINGS := -Wall -Wextra -pedantic
OPTIMIZE := -O3 -march=native -flto -funroll-loops
DEBUG_FLAGS := -g -rdynamic -O0 -DDEBUG -fno-omit-frame-pointer -fno-inline
PROFILE_FLAGS := -pg -O2 -g

# Common flags for all builds
COMMON_FLAGS := $(STD) $(WARNINGS) -MMD $(addprefix -I,$(INC_DIRS))

# Release configuration
CFLAGS_RELEASE := $(COMMON_FLAGS) $(OPTIMIZE) \
                 -fomit-frame-pointer \
                 -fno-signed-zeros -fno-trapping-math \
                 -fassociative-math -fno-math-errno \
                 -fstrict-aliasing -ftree-vectorize \
                 -fno-stack-protector
LDFLAGS_RELEASE := $(OPTIMIZE) -fuse-linker-plugin

# Debug configuration
CFLAGS_DEBUG := $(COMMON_FLAGS) $(DEBUG_FLAGS)
LDFLAGS_DEBUG := $(DEBUG_FLAGS)

# Profile configuration
CFLAGS_PROFILE := $(COMMON_FLAGS) $(PROFILE_FLAGS)
LDFLAGS_PROFILE := $(PROFILE_FLAGS)

# ===== Targets =====
COMPRESS_TARGET := compress
DECOMPRESS_TARGET := decompress
DEBUG_COMPRESS_TARGET := compress-debug
PROFILE_COMPRESS_TARGET := compress-profile

# ===== Source Files =====
include used_sources.mk

COMPRESS_SRCS := $(filter-out $(SRC_DIR)/decompress/decompress.c, $(SRCS))
DECOMPRESS_SRCS := $(SRC_DIR)/decompress/decompress.c

# ===== Object Files =====
COMPRESS_RELEASE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/release/%.o,$(COMPRESS_SRCS))
COMPRESS_DEBUG_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/debug/%.o,$(COMPRESS_SRCS))
COMPRESS_PROFILE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/profile/%.o,$(COMPRESS_SRCS))
DECOMPRESS_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/release/%.o,$(DECOMPRESS_SRCS))

# ===== Dependency Files =====
DEPS := $(COMPRESS_RELEASE_OBJS:.o=.d) $(COMPRESS_DEBUG_OBJS:.o=.d) \
        $(DECOMPRESS_OBJ:.o=.d) $(COMPRESS_PROFILE_OBJS:.o=.d)

# ===== Phony Targets =====
.PHONY: all release debug profile compress decompress clean help

# ===== Build Rules =====
all: compress decompress

release: compress decompress
	@echo "Built release versions: ./$(COMPRESS_TARGET) and ./$(DECOMPRESS_TARGET)"

debug: $(DEBUG_COMPRESS_TARGET) decompress
	@echo "Built debug version: ./$(DEBUG_COMPRESS_TARGET) and ./$(DECOMPRESS_TARGET)"

profile: $(PROFILE_COMPRESS_TARGET)
	@echo "Built profiling version: ./$(PROFILE_COMPRESS_TARGET) (use with gprof)"

compress: $(COMPRESS_RELEASE_OBJS)
	$(CC) $(LDFLAGS_RELEASE) -o $(COMPRESS_TARGET) $^ -lm
	@echo "Built compression tool: ./$(COMPRESS_TARGET)"
	@size $(COMPRESS_TARGET)

decompress: $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS_RELEASE) -o $(DECOMPRESS_TARGET) $^ -lm
	@echo "Built decompression tool: ./$(DECOMPRESS_TARGET)"
	@size $(DECOMPRESS_TARGET)

$(DEBUG_COMPRESS_TARGET): $(COMPRESS_DEBUG_OBJS)
	$(CC) $(LDFLAGS_DEBUG) -o $@ $^ -lm
	@echo "Built debug compression tool: ./$@"
	@size $@

$(PROFILE_COMPRESS_TARGET): $(COMPRESS_PROFILE_OBJS)
	$(CC) $(LDFLAGS_PROFILE) -o $@ $^ -lm
	@echo "Built profiling binary: $@"
	@size $@

# ===== Compilation Rules =====
$(BUILD_DIR)/release/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/debug/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILD_DIR)/profile/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_PROFILE) -c $< -o $@

# ===== Clean =====
clean:
	@rm -rf $(BUILD_DIR) $(COMPRESS_TARGET) $(DEBUG_COMPRESS_TARGET) \
	        $(DECOMPRESS_TARGET) $(PROFILE_COMPRESS_TARGET)
	@echo "Cleaned all build artifacts"

# ===== Dependencies =====
-include $(DEPS)

# ===== Help =====
help:
	@echo "Available targets:"
	@echo "  all         - Build both tools (default)"
	@echo "  release     - Build optimized versions of both tools"
	@echo "  debug       - Build debug version of compressor and release decompressor"
	@echo "  profile     - Build profiling version of compressor for gprof"
	@echo "  compress    - Build only compression tool"
	@echo "  decompress  - Build only decompression tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  help        - Show this help message"