#pragma once

#include <vector>
#include <cstdint>
#include "lazymc/lazygraph.h"  // for AdjList

namespace lazymc {

// Computes the coreness (k-core) values for all vertices of G.
// Parameter min_degree is |C*| in Algorithm 1.
// If min_degree == 0, the full k-core decomposition is computed.
std::vector<int32_t> KCore(const AdjList& G, int32_t min_degree);

// Core numbers of the vertices in the k-core of G; 0 for all other vertices.
//
// Only vertices of core >= k can be part of a clique with more than k
// vertices, and their core numbers are the same in G and in its k-core, so
// it is enough to peel G down to its k-core and decompose that. Vertices of
// degree < k are dropped without touching their edges, so the cost is
// O(n + sum of the degrees of vertices with degree >= k) instead of O(n + m).
std::vector<int32_t> KCoreAtLeast(const AdjList& G, int32_t k);

} // namespace lazymc
