# src/files/decompression/Makefile
CC = gcc

# Base include flags
INCLUDES = -I../../files \
           -I../../files/compression \
           -I../../files/decompression

# Check for AVX/SSE
AVX2 := $(shell grep avx2 /proc/cpuinfo 2>/dev/null)
SSE4 := $(shell grep sse4_2 /proc/cpuinfo 2>/dev/null)
EXTRA_CPU_FLAGS =
ifneq ($(AVX2),)
    EXTRA_CPU_FLAGS += -mavx2 -mbmi2
endif
ifneq ($(SSE4),)
    EXTRA_CPU_FLAGS += -msse4.2
endif

# Support debug build from parent Makefile
ifeq ($(DEBUG),1)
    CFLAGS = -g -O0 -DDEBUG -rdynamic -fno-omit-frame-pointer -fno-inline \
             -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(INCLUDES) $(EXTRA_CPU_FLAGS)
    LDFLAGS =
    TARGET := $(abspath ../../../decompress-debug)
else
    CFLAGS = -O3 -march=native -flto -fomit-frame-pointer \
             -funroll-loops -fprefetch-loop-arrays \
             -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
             $(INCLUDES) $(EXTRA_CPU_FLAGS)
    LDFLAGS = -flto -Wl,-O3 -Wl,--as-needed
    TARGET := $(abspath ../../../decompress)
endif

include decompress_sources.mk

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS) -lm


clean:
	rm -f $(abspath ../../../decompress*) *.o
