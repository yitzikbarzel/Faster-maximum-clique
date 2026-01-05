#pragma once

#include <cstdint>
#include <vector>

namespace lazymc {

using AdjList = std::vector<std::vector<int32_t>>;

struct McqdResult {
    std::vector<int32_t> clique; // vertices 0..n-1
    long long steps = 0;         // mcqd steps() if available
};

McqdResult mcqd_max_clique(const AdjList& G, double dyn_fraction = 0.0);

} // namespace lazymc
