#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h" // for AdjList

namespace lazymc {

struct SortOrder {
    // relabel_to_original[v_H] = v_G
    std::vector<int32_t> relabel_to_original;

    // original_to_relabel[v_G] = v_H
    std::vector<int32_t> original_to_relabel;
};

// Determines a relabeling (order) for vertices in H based on core values.
// This is Algorithm 1, Step 5.
SortOrder DetermineSortOrder(const AdjList& G, const std::vector<int32_t>& core);

} // namespace lazymc
