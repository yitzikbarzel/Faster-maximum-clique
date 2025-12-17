#include "lazymc/lazymc.h"
#include "lazymc/lazygraph.h"
#include "lazymc/kcore.h"
#include "lazymc/determine_sort_order.h"
#include "lazymc/degree_heuristic.h"
#include "lazymc/coreness_heuristic.h"
#include "lazymc/systematic_search.h"

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

    // Step 3: DegreeBasedHeuristicSearch(G)
{ 
    std::vector<int32_t> clique = degree_based_heuristic(G, 128);

    if (clique.size() > g_incumbent_clique.size()) {
        g_incumbent_clique = std::move(clique);
    }
}


    // Step 4: KCore(G, |C*|)
    const int32_t clique_lb = static_cast<int32_t>(g_incumbent_clique.size());
    std::vector<int32_t> core = KCore(G, clique_lb);


    // Step 5: DetermineSortOrder(G, core)
    const SortOrder order = DetermineSortOrder(G, core);
    const std::vector<int32_t>& relabel_to_original = order.relabel_to_original;
    const std::vector<int32_t>& original_to_relabel = order.original_to_relabel;


    // Step 6: build LazyGraph H from G, order o, and core values.
    LazyGraph H(G, relabel_to_original, original_to_relabel, core);
{
    // Step 7: CorenessBasedHeuristicSearch(H, core)
    std::vector<int32_t> clique = coreness_based_heuristic(H, core);
    if (clique.size() > g_incumbent_clique.size()) {
        g_incumbent_clique = std::move(clique);
    }
}

   // Step 8: SystematicSearch(H, core)
{
    std::vector<int32_t> clique = systematic_search(H, core);
    if (clique.size() > g_incumbent_clique.size()) {
        g_incumbent_clique = std::move(clique);
    }
}


    // For now we simply return the current incumbent clique (possibly empty).
    return g_incumbent_clique;
}

} // namespace lazymc
