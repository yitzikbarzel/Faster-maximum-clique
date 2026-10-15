#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h"

namespace lazymc {

class Incumbent;

// Coreness-based heuristic search (Algorithm 6 style).
// Improvements are written to `incumbent`; returns the incumbent clique
// afterwards, in G-labels.
std::vector<int32_t> coreness_based_heuristic(LazyGraph& H,
                                             const std::vector<int32_t>& core,
                                             Incumbent& incumbent);

// Same, starting from an empty incumbent.
std::vector<int32_t> coreness_based_heuristic(LazyGraph& H,
                                             const std::vector<int32_t>& core);

} // namespace lazymc
