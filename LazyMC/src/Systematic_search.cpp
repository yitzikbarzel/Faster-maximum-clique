
#include "lazymc/systematic_search.h"
#include "lazymc/neighbor_search.h"
#include "lazymc/incumbent.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lazymc {

std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core_H) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return {};

    // degeneracy + 1 ≈ max core value
    int32_t max_core = 0;
    for (int32_t v = 0; v < n; ++v) {
        max_core = std::max(max_core, core_H[static_cast<std::size_t>(v)]);
    }

    std::vector<int32_t>& best_clique_G = g_incumbent_clique;
    sync_incumbent_size();
    int32_t best_size = static_cast<int32_t>(best_clique_G.size());

    constexpr double phi = 0.7; // tunable

    // Lines 2-5: warm-up pass
    // parfor k ← |C*| to degeneracy+1:
    for (int32_t k = static_cast<int32_t>(best_clique_G.size()); k <= max_core; ++k) {
        if (k < static_cast<int32_t>(best_clique_G.size())) continue;
        // v ← lowest-numbered vertex with c[v] = k
        int32_t v = -1;
        for (int32_t i = 0; i < n; ++i) {
            if (core_H[static_cast<std::size_t>(i)] == k) {
                v = i;
                break;
            }
        }
        if (v >= 0) {
            const int32_t before = static_cast<int32_t>(best_clique_G.size());
            NeighborSearch(H, v, core_H, best_clique_G, phi);
            if (static_cast<int32_t>(best_clique_G.size()) > before) {
                best_size = static_cast<int32_t>(best_clique_G.size());
                g_best_size.store(best_size, std::memory_order_relaxed);
            }
        }
    }

    // Lines 6-11: main pass
    for (int32_t k = max_core; k >= 1; --k) {
        if (k < static_cast<int32_t>(best_clique_G.size())) continue;

        for (int32_t v = 0; v < n; ++v) {
            if (core_H[static_cast<std::size_t>(v)] != k) continue;
            if (core_H[static_cast<std::size_t>(v)] < static_cast<int32_t>(best_clique_G.size())) continue;

            const int32_t before = static_cast<int32_t>(best_clique_G.size());
            NeighborSearch(H, v, core_H, best_clique_G, phi);
            if (static_cast<int32_t>(best_clique_G.size()) > before) {
                best_size = static_cast<int32_t>(best_clique_G.size());
                g_best_size.store(best_size, std::memory_order_relaxed);
            }
        }
    }

    return best_clique_G;
}
}
