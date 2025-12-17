#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h"

namespace lazymc {

// Coreness-based heuristic search (Algorithm 6 style).
// Returns a clique as a list of vertices in G-labels.
std::vector<int32_t> coreness_based_heuristic(LazyGraph& H,
                                             const std::vector<int32_t>& core);

} // namespace lazymc
