#include "top_savings.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    TopSavingNode nodes[MAX_TOP_SAVINGS];
    int size;
} TopSavingHeap;


static TopSavingHeap heap;

static inline void swap(TopSavingNode *a, TopSavingNode *b) {
    TopSavingNode tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heapify_up(int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heap.nodes[idx].savings < heap.nodes[parent].savings) {
            swap(&heap.nodes[idx], &heap.nodes[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapify_down(int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < heap.size && heap.nodes[left].savings < heap.nodes[smallest].savings)
        smallest = left;
    if (right < heap.size && heap.nodes[right].savings < heap.nodes[smallest].savings)
        smallest = right;

    if (smallest != idx) {
        swap(&heap.nodes[idx], &heap.nodes[smallest]);
        heapify_down(smallest);
    }
}

void init_top_savings(void) {
    heap.size = 0;
}

static bool sequences_match(const TopSavingNode *n, const uint8_t *seq, uint8_t len) {
    return (n->length == len) && (memcmp(n->sequence, seq, len) == 0);
}

void try_insert_top_saving(const uint8_t *seq, uint8_t len, uint32_t freq, uint32_t node_id) {
    if (len < 2 || freq <= 1) return;
    uint32_t savings = len * freq;

    // 1. Look for existing entry
    for (int i = 0; i < heap.size; i++) {
        if (sequences_match(&heap.nodes[i], seq, len)) {
            if (heap.nodes[i].node_count < MAX_NODES_PER_SEQ)
                heap.nodes[i].node_ids[heap.nodes[i].node_count++] = node_id;
            return;
        }
    }

    // 2. Insert new
    if (heap.size < MAX_TOP_SAVINGS) {
        heap.nodes[heap.size] = (TopSavingNode){
            .sequence = seq, .length = len, .frequency = freq,
            .savings = savings, .node_count = 1
        };
        heap.nodes[heap.size].node_ids[0] = node_id;
        heapify_up(heap.size++);
    } else if (savings > heap.nodes[0].savings) {
        heap.nodes[0] = (TopSavingNode){
            .sequence = seq, .length = len, .frequency = freq,
            .savings = savings, .node_count = 1
        };
        heap.nodes[0].node_ids[0] = node_id;
        heapify_down(0);
    }
}


void print_top_savings(void) {
    printf("Top %d savings sequences:\n", heap.size);
    for (int i = 0; i < heap.size; i++) {
        const TopSavingNode *n = &heap.nodes[i];
        printf("Savings=%u, Len=%u, Freq=%u, Seq=\"", n->savings, n->length, n->frequency);
        for (int j = 0; j < n->length; j++) {
            printf("%c", n->sequence[j]);
        }
        printf("\"\n");
    }
}
