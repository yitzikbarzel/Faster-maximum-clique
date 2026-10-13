#pragma once

#include <cstdint>
#include <vector>

#include "lazymc/lazygraph.h"

namespace lazymc {

class Incumbent;

// Algorithm 7: Systematic search over neighborhoods (calls NeighborSearch).
// Only cliques larger than `incumbent` are searched for; improvements are
// written to `incumbent`. Returns the incumbent clique at the end (G-labels).
std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core_H,
                                       Incumbent& incumbent);

// Same, starting from an empty incumbent.
std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core_H);

} // namespace lazymc
