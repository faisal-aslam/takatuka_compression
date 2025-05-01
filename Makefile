# Main Makefile - includes specific build files

# Default target
all: compress decompress

# Include specific build files
include compress.mk
include decompress.mk

.PHONY: all clean help

clean:
	@rm -rf build $(COMPRESS_TARGET) $(DEBUG_COMPRESS_TARGET) $(DECOMPRESS_TARGET)
	@echo "Cleaned all build artifacts"

help:
	@echo "Available targets:"
	@echo "  all         - Build both tools (default)"
	@echo "  release     - Build optimized versions of both tools"
	@echo "  debug       - Build debug version of compressor and release decompressor"
	@echo "  compress    - Build only compression tool"
	@echo "  decompress  - Build only decompression tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  help        - Show this help message"