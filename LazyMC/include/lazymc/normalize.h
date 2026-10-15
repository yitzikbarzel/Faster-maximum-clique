#pragma once

#include "lazymc/lazygraph.h" // for AdjList

namespace lazymc {

// True if every adjacency list is sorted, duplicate-free, in range and has no
// self-loop. This is what the solvers expect. O(n + m).
bool is_normalized(const AdjList& G);

// Sorted, deduplicated, symmetric copy of G without self-loops or
// out-of-range ids.
AdjList normalized_copy(const AdjList& G);

} // namespace lazymc
