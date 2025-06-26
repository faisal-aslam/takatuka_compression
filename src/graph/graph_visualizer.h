#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include <stdio.h>
#include <stdint.h>

/**
 * Visualizes the graph using DOT language
 * @param block The data block containing sequence information
 */
void visualize_graph(const uint8_t* block);

#endif // GRAPH_VISUALIZER_H