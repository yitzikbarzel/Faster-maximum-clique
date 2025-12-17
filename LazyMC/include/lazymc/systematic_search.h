#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h"

namespace lazymc {

// Systematic search (Algorithm 7 in the paper).
// Returns a clique as a list of vertices in G-labels.
std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core);

} // namespace lazymc
