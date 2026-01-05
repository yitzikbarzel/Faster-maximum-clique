#pragma once
#include <cstdint>
#include <vector>

namespace lazymc {

class LazyGraph;

// Returns true iff found a clique strictly larger than incumbent_size.
// If true, writes the found clique (in G-labeling) into out_clique_G.
bool NeighborSearch(LazyGraph& H,
                    int32_t vH,
                    const std::vector<int32_t>& core_H,
                    int32_t incumbent_size,
                    double phi,
                    std::vector<int32_t>& out_clique_G);

} // namespace lazymc
