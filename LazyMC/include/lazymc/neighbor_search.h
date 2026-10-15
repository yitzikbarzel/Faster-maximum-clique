#pragma once
#include <cstdint>
#include <vector>

namespace lazymc {

class LazyGraph;
class Incumbent;

// Searches for a clique larger than `incumbent_size` among v and its right
// neighbors (Algorithm 7, NeighborSearch).
// Returns true iff such a clique was found; it is then written (in G-labels)
// into out_clique_G and is a maximum clique of G[{v} ∪ N+(v)].
bool NeighborSearch(LazyGraph& H,
                    int32_t vH,
                    const std::vector<int32_t>& core_H,
                    int32_t incumbent_size,
                    double phi,
                    std::vector<int32_t>& out_clique_G);

// Same, with the threshold taken from a shared incumbent. Other threads may
// grow the incumbent during the call, which tightens pruning; the result is
// then only guaranteed to be larger than the incumbent at the time of the
// call.
bool NeighborSearch(LazyGraph& H,
                    int32_t vH,
                    const std::vector<int32_t>& core_H,
                    const Incumbent& incumbent,
                    double phi,
                    std::vector<int32_t>& out_clique_G);

} // namespace lazymc
