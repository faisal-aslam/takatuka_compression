// lzss_cost.h
//
// A standalone, self-contained (length, distance) cost model for LZSS-style
// back-references, ready to be picked up by shortest_path_dp.c the moment
// your matchfinder starts populating GraphNode->distance.
//
// -----------------------------------------------------------------------
// Why (length, distance) instead of the old calc_cost(node, frequency)
// -----------------------------------------------------------------------
// Under the dictionary/header scheme, a repeated sequence's cost depended
// on how many times it repeated ACROSS THE WHOLE FILE (frequency) -- which
// is what made the flat additive DP a slightly-wrong proxy for the real
// cost (it couldn't see the one-time header/dictionary-registration cost).
//
// Under pure LZSS back-references, a match token is entirely self-
// contained: its cost is a function of its OWN length and its OWN
// distance back to the copy source, full stop. Nothing shared, nothing
// amortized, no hidden fixed cost anywhere else in the file. That is
// exactly the assumption the DP's optimality proof needs -- so once your
// graph carries real distances, this cost model makes the DP exactly
// optimal, not just optimal-for-an-approximation.
//
// -----------------------------------------------------------------------
// The cost unit: bits, not the old ad-hoc "1/2/5" byte-ish units
// -----------------------------------------------------------------------
// Real back-reference formats (DEFLATE, LZMA, ...) spend a variable
// number of BITS on length and distance -- short/near ones are cheap,
// long/far ones cost more. Returning bits (rather than bytes) gives the DP
// a much more accurate signal to compare literal-vs-match with, and costs
// compose additively exactly the way the DP needs (total bits = sum of
// per-token bits), so nothing else about the DP has to change.
//
// The exact codeword lengths here are an ESTIMATE (a standard Elias-gamma-
// style variable length code), not necessarily what your actual bit_writer/
// range coder will produce -- that's fine and expected. The point of this
// cost model is to guide the PARSE (which chunk boundaries to choose), not
// to predict the final file size to the bit. Once you have a real entropy
// coder (item 3), the standard refinement loop is: parse with this
// estimate, measure the real bits the entropy coder actually spent, adjust,
// re-parse, repeat 2-4 rounds until it stabilizes (see the "iterative price
// refinement" note from our design discussion).

#pragma once

#include <stdint.h>

// Minimum length worth encoding as a match at all (shorter than this,
// always prefer literals). Matches SEQ_LENGTH_START in constants.h.
#ifndef LZSS_MIN_MATCH
#define LZSS_MIN_MATCH SEQ_LENGTH_START
#endif

// Defined in shortest_path_dp.c, defaults to 0 (today's dictionary-style
// costing). Set to 1 from your matchfinder/graph-construction code once
// GraphNode->distance is genuinely populated for LZSS back-references --
// see the comment on dp_node_cost() in shortest_path_dp.c for exactly what
// this changes.
extern uint8_t use_lzss_cost_model;

/**
 * Number of bits needed to represent a positive integer x (x >= 1) in
 * plain binary, i.e. floor(log2(x)) + 1.
 */
static inline uint32_t lzss_bits_needed(uint32_t x) {
    uint32_t b = 0;
    while (x) {
        b++;
        x >>= 1;
    }
    return b;
}

/**
 * Approximate cost, in bits, of a self-delimiting (prefix-free) code for a
 * positive integer x -- classic Elias-gamma shape: a unary length prefix
 * (so the decoder knows how many bits follow) plus the binary remainder.
 * This is a good stand-in for "small numbers are cheap, big numbers cost
 * log(n) more bits", which is the only property the DP actually needs.
 */
static inline uint32_t lzss_var_int_cost_bits(uint32_t x) {
    if (x == 0) x = 1; // avoid log(0); callers should only pass x >= 1 anyway
    uint32_t b = lzss_bits_needed(x);
    return 2 * b - 1;
}

/**
 * Cost, in bits, of emitting one literal byte (1 flag bit + 8 data bits).
 */
static inline uint32_t lzss_literal_cost_bits(void) {
    return 1u + 8u;
}

/**
 * Cost, in bits, of emitting one match token of the given length and
 * distance. `length` must be >= LZSS_MIN_MATCH; `distance` must be >= 1
 * (how many bytes back the copy source starts, i.e. source_offset =
 * this_node_offset - distance).
 *
 * length is re-based to start at 1 (length - LZSS_MIN_MATCH + 1) so the
 * *shortest legal match* costs the least, matching how real formats index
 * their length codes from the minimum match length rather than from 0.
 */
static inline uint32_t lzss_match_cost_bits(uint32_t length, uint32_t distance) {
    uint32_t len_code = (length >= LZSS_MIN_MATCH) ? (length - LZSS_MIN_MATCH + 1u) : 1u;
    if (distance == 0) distance = 1; // defensive; a real distance is always >= 1
    return 1u /* flag bit */ + lzss_var_int_cost_bits(len_code) + lzss_var_int_cost_bits(distance);
}
