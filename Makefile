CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops -fomit-frame-pointer -MMD -I./src
LDFLAGS = -flto -O3
TARGET = weighted_freq
SRC_DIR = src
BUILD_DIR = build

# List all source files (with path relative to SRC_DIR)
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/second_pass/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

.PHONY: all clean release debug

all: $(TARGET)

release: CFLAGS += -DNDEBUG
release: $(TARGET)

debug: CFLAGS += -O0 -g -DDEBUG
debug: LDFLAGS = -g
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Generic rule for all .c files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(BUILD_DIR)
	rm -f $(SRC_DIR)/combined_code.c

-include $(DEPS)
