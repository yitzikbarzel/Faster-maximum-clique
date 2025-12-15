#include "lazymc/determine_sort_order.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lazymc {

SortOrder DetermineSortOrder(const AdjList& G,
                             const std::vector<int32_t>& core) {
    const std::size_t n = G.size();

    // Find maximum core value to size buckets
    int32_t max_core = 0;
    for (std::size_t v = 0; v < n; ++v) {
        if (core[v] > max_core) {
            max_core = core[v];
        }
    }

    // Buckets: bucket[c] contains vertices with core == c
    std::vector<std::vector<int32_t>> buckets(static_cast<std::size_t>(max_core) + 1);
    for (std::size_t v = 0; v < n; ++v) {
        const int32_t c = core[v];
        if (c < 0) continue; // safety
        buckets[static_cast<std::size_t>(c)].push_back(static_cast<int32_t>(v));
    }

    SortOrder result;
    result.relabel_to_original.resize(n);
    result.original_to_relabel.resize(n);

    // Read buckets from high core to low core.
    // Inside each bucket we keep insertion order (increasing v),
    // which is deterministic and avoids any extra sorting cost.
    std::size_t out = 0;
    for (std::size_t c = buckets.size(); c-- > 0;) {
        for (int32_t v_G : buckets[c]) {
            result.relabel_to_original[out] = v_G;
            result.original_to_relabel[static_cast<std::size_t>(v_G)] =
                static_cast<int32_t>(out);
            ++out;
        }
    }

    return result;
}

} // namespace lazymc
