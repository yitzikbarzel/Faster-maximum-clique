#pragma once

#include <cstdint>
#include <vector>

namespace lazymc {

// Simple adjacency list: G[v] = neighbors of v
using AdjList = std::vector<std::vector<int32_t>>;

// Result struct for k-Vertex-Cover.
struct k_vertex_cover_res {
    bool found = false;               // true if a cover of size <= k exists
    bool aborted = false;             // true if search stopped due to step limit
    std::vector<int32_t> cover;       // vertices in the found cover (indices)
};

// Solve k-Vertex-Cover on an adjacency-list graph.
// Returns:
//   - found = true  => cover.size() <= k, and "cover" contains such a cover
//   - found = false && aborted = false  => הוכיח שאין cover בגודל <= k
//   - found = false && aborted = true   => החיפוש הופסק בגלל מגבלת צעדים
k_vertex_cover_res k_vertex_cover_adj(const AdjList& G, int32_t k);

} // namespace lazymc
