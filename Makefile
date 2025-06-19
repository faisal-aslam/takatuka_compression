# === Compiler and Flags ===
CC = gcc
STD = -std=c17
BUILD_DIR = build

# === Release Flags (Optimized) ===
CFLAGS_RELEASE = $(STD) -Wall -Wextra -pedantic -O3 -march=native -flto \
                 -funroll-loops -fomit-frame-pointer -MMD -I./src \
                 -fno-signed-zeros -fno-trapping-math -fassociative-math \
                 -fno-math-errno -fstrict-aliasing -ftree-vectorize \
                 -fno-stack-protector
LDFLAGS_RELEASE = -flto -O3 -fuse-linker-plugin

# === Debug Flags ===
CFLAGS_DEBUG = $(STD) -Wall -Wextra -pedantic -g -rdynamic -O0 -I./src -MMD \
               -DDEBUG -fno-omit-frame-pointer -fno-inline
LDFLAGS_DEBUG = -g -rdynamic

# === Profiling Flags (for gprof) ===
CFLAGS_PROFILE = $(STD) -Wall -Wextra -pedantic -pg -O2 -g -I./src -MMD
LDFLAGS_PROFILE = -pg -g

# === Executable Targets ===
COMPRESS_TARGET = compress
DECOMPRESS_TARGET = decompress
DEBUG_COMPRESS_TARGET = compress-debug
PROFILE_COMPRESS_TARGET = compress-profile

# === Source Files ===
include used_sources.mk

COMPRESS_SRCS = $(filter-out src/decompress/decompress.c, $(SRCS))
DECOMPRESS_SRCS = src/decompress/decompress.c

# === Object Files by Mode ===
COMPRESS_RELEASE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(COMPRESS_SRCS))
COMPRESS_DEBUG_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/debug/%.o,$(COMPRESS_SRCS))
COMPRESS_PROFILE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/profile/%.o,$(COMPRESS_SRCS))
DECOMPRESS_OBJ = $(patsubst src/%.c,$(BUILD_DIR)/release/%.o,$(DECOMPRESS_SRCS))

# === Dependency Files ===
DEPS = $(COMPRESS_RELEASE_OBJS:.o=.d) $(COMPRESS_DEBUG_OBJS:.o=.d) \
       $(DECOMPRESS_OBJ:.o=.d) $(COMPRESS_PROFILE_OBJS:.o=.d)

# === Phony Targets ===
.PHONY: all release debug profile compress decompress clean help

# === Default Target ===
all: compress decompress

# === Build Targets ===
release: compress decompress
	@echo "Built release versions: ./compress and ./decompress"

debug: $(DEBUG_COMPRESS_TARGET) decompress
	@echo "Built debug version: ./compress-debug and ./decompress"

profile: $(PROFILE_COMPRESS_TARGET)
	@echo "Built profiling version: ./compress-profile (use with gprof)"

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

$(PROFILE_COMPRESS_TARGET): $(COMPRESS_PROFILE_OBJS)
	$(CC) $(LDFLAGS_PROFILE) -o $@ $^ -lm
	@echo "Built profiling binary: $@"
	@echo "Static memory usage (compress-profile):"
	@size $@

# === Compilation Rules ===
$(BUILD_DIR)/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(BUILD_DIR)/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(BUILD_DIR)/profile/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_PROFILE) -c $< -o $@

# === Clean Target ===
clean:
	@rm -rf $(BUILD_DIR) $(COMPRESS_TARGET) $(DEBUG_COMPRESS_TARGET) \
	        $(DECOMPRESS_TARGET) $(PROFILE_COMPRESS_TARGET)
	@echo "Cleaned all build artifacts"

# === Auto-Include Dependencies ===
-include $(DEPS)

# === Help Target ===
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
