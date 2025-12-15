#pragma once

#include <vector>
#include <cstdint>
#include "lazymc/lazygraph.h"  // for AdjList

namespace lazymc {

// Computes the coreness (k-core) values for all vertices of G.
// Parameter min_degree is |C*| in Algorithm 1.
// If min_degree == 0, the full k-core decomposition is computed.
std::vector<int32_t> KCore(const AdjList& G, int32_t min_degree);

} // namespace lazymc
