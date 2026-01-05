#pragma once

#include <cstdint>
#include <vector>

#include "lazymc/lazygraph.h"

namespace lazymc {

// Algorithm 7: Systematic search over neighborhoods (calls NeighborSearch).
// Returns best clique found (in ORIGINAL G-labeling).
std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core_H);

} // namespace lazymc
