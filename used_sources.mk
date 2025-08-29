# This is used_sources.mk which lists all actually used .c source files
SRCS = \
    src/decompress/decompress.c \
    src/map/xxhash.c \
    src/map/seq_freq_map.c \
    src/files/compression/compress.c \
    src/time.c \
    src/graph/shortest_path/shortest_path_greedy.c \
    src/graph/graph.c \
    src/graph/graph_visualizer.c \
    src/files/best_path_view.c \
    src/files/compression/compressed_header.c \
    src/files/compression/compressed_body.c \
    src/files/compression/code_map.c \
    src/files/code_classes.c \
    src/files/compression/bit_writer.c \
    src/logic.c \
    src/main.c