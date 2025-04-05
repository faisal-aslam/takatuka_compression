# Decompress-specific optimizations
CC = gcc
CFLAGS = -O3 -march=native -flto -fomit-frame-pointer \
         -funroll-loops -fprefetch-loop-arrays \
         -fno-stack-protector -D_FORTIFY_SOURCE=0 \
         -I../..

LDFLAGS = -flto -Wl,-O3 -Wl,--as-needed

# Detect AVX/SSE support
AVX2 := $(shell grep avx2 /proc/cpuinfo 2>/dev/null)
ifneq ($(AVX2),)
    CFLAGS += -mavx2 -mbmi2
endif

SSE4 := $(shell grep sse4 /proc/cpuinfo 2>/dev/null)
ifneq ($(SSE4),)
    CFLAGS += -msse4.2
endif

SRC = decompress.c
TARGET = decompress

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
