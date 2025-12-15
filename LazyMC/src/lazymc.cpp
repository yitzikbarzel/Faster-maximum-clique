#include "lazymc/lazymc.h"
#include "lazymc/lazygraph.h"
#include "lazymc/kcore.h"


#include <cstddef>
#include <vector>
#include <cstdint>

namespace lazymc {

namespace {
// Global incumbent clique, as in Algorithm 1 (C*).
// For now it is only used inside LazyMC_run.
std::vector<int32_t> g_incumbent_clique;
} // namespace

std::vector<int32_t> LazyMC_run(const AdjList& G) {
    // Step 2: initialize incumbent clique C* = {}
    g_incumbent_clique.clear();

    // TODO (Step 3): DegreeBasedHeuristicSearch(G)
    // This should update g_incumbent_clique when implemented.

    const std::size_t n = G.size();

    // Step 4: KCore(G, |C*|)
    const int32_t clique_lb = static_cast<int32_t>(g_incumbent_clique.size());
    std::vector<int32_t> core = KCore(G, clique_lb);


    // Step 5: DetermineSortOrder(G, c)
    // For now we use identity order: H-labels == G-labels.
    std::vector<int32_t> relabel_to_original(n);
    std::vector<int32_t> original_to_relabel(n);

    for (std::size_t v = 0; v < n; ++v) {
        relabel_to_original[v] = static_cast<int32_t>(v); // H -> G
        original_to_relabel[v] = static_cast<int32_t>(v); // G -> H
    }

    // Step 6: build LazyGraph H from G, order o, and core values.
    LazyGraph H(G, relabel_to_original, original_to_relabel, core);

    // TODO (Step 7): CorenessBasedHeuristicSearch(H, core)
    // This should also update g_incumbent_clique.

    // TODO (Step 8): SystematicSearch(H, core)
    // This should also update g_incumbent_clique.

    // For now we simply return the current incumbent clique (possibly empty).
    return g_incumbent_clique;
}

} // namespace lazymc
