CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
         -fomit-frame-pointer -MMD -I./src \
         -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
         $(CPUFLAGS)
CFLAGS += -fstrict-aliasing -ftree-vectorize -fno-stack-protector
LDFLAGS = -flto -O3 -fuse-linker-plugin
TARGET = weighted_freq
SRC_DIR = src
BUILD_DIR = build

# List all source files
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/second_pass/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

.PHONY: all clean release debug pgo

all: $(TARGET)

release: CFLAGS += -DNDEBUG
release: $(TARGET)

debug: CFLAGS += -O0 -g -DDEBUG
debug: LDFLAGS = -g
debug: $(TARGET)

pgo:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-generate" LDFLAGS="$(LDFLAGS) -fprofile-generate" $(TARGET)
	./$(TARGET) your_input_file
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-use" LDFLAGS="$(LDFLAGS) -fprofile-use" $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(BUILD_DIR)
	rm -f $(SRC_DIR)/combined_code.c

-include $(DEPS)
