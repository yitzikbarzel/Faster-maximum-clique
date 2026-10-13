#include "lazymc/normalize.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace lazymc {

bool is_normalized(const AdjList& G) {
    const auto n = static_cast<int32_t>(G.size());
    for (int32_t v = 0; v < n; ++v) {
        int32_t prev = -1;
        for (int32_t u : G[static_cast<std::size_t>(v)]) {
            if (u <= prev || u >= n || u == v) return false;
            prev = u;
        }
    }
    return true;
}

AdjList normalized_copy(const AdjList& G) {
    const auto n = static_cast<int32_t>(G.size());
    AdjList out(G.size());
    for (int32_t v = 0; v < n; ++v) {
        for (int32_t u : G[static_cast<std::size_t>(v)]) {
            if (u < 0 || u >= n || u == v) continue;
            out[static_cast<std::size_t>(v)].push_back(u);
            out[static_cast<std::size_t>(u)].push_back(v);
        }
    }
    for (auto& nbrs : out) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }
    return out;
}

} // namespace lazymc
