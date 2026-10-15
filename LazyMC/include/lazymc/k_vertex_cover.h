#pragma once

#include <cstdint>
#include <vector>

namespace lazymc {

// Simple adjacency list: G[v] = neighbors of v
using AdjList = std::vector<std::vector<int32_t>>;

// Result struct for k-Vertex-Cover.
struct k_vertex_cover_res {
    bool found = false;         // true if a cover of size <= k exists
    bool aborted = false;       // true if the search was aborted (step limit or invalid input)
    std::vector<int32_t> cover; // vertices in the found cover (0-based indices)
};

// Solve k-Vertex-Cover on an adjacency-list graph.
//
// Parameters:
//   G - adjacency list of an undirected graph, 0-based vertex indices
//   k - target size for the vertex cover
//
// Returns:
//   - found = true,  aborted = false:
//       A vertex cover of size <= k was found. 'cover' contains such a cover.
//   - found = false, aborted = false:
//       It was proven that no vertex cover of size <= k exists.
//   - found = false, aborted = true:
//       The algorithm was aborted (for example, step limit exceeded or invalid input detected).
k_vertex_cover_res k_vertex_cover_adj(const AdjList& G, int32_t k);

// Result of a minimum vertex cover search.
struct min_vertex_cover_res {
    bool found = false;         // a cover of size <= k_max was found
    bool optimal = false;       // `cover` is proven minimum (implies found)
    bool aborted = false;       // step limit hit or invalid input
    std::vector<int32_t> cover; // smallest cover found
};

// Finds a minimum vertex cover of G, provided one of size <= k_max exists.
//
//   - found && optimal:  `cover` is a minimum vertex cover.
//   - !found && !aborted: proven that no cover of size <= k_max exists.
//   - aborted: the step limit was hit; if `found`, `cover` is the best cover
//     seen so far but it is not proven minimum.
min_vertex_cover_res min_vertex_cover_adj(const AdjList& G,
                                          int32_t k_max,
                                          long long step_limit = 20'000'000);

} // namespace lazymc
