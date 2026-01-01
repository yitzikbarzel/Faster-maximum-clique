// #include "lazymc/lazymc.h"
// #include "lazymc/lazygraph.h"
// #include "lazymc/kcore.h"
// #include "lazymc/determine_sort_order.h"
// #include "lazymc/degree_heuristic.h"
// #include "lazymc/coreness_heuristic.h"
// #include "lazymc/systematic_search.h"
// #include "lazymc/incumbent.h"

// #include <cstddef>
// #include <vector>
// #include <cstdint>
// #include <atomic>

// namespace lazymc {

// // Global incumbent clique, as in Algorithm 1 (C*).
// std::vector<int32_t> g_incumbent_clique;
// std::atomic<int32_t> g_best_size{0};

// std::vector<int32_t> LazyMC_run(const AdjList& G) {
//     // Step 2: initialize incumbent clique C* = {}
//     reset_incumbent();

//   //  Step 3: DegreeBasedHeuristicSearch(G)
// { 
//     std::vector<int32_t> clique = degree_based_heuristic(G, 120);

//     try_update_incumbent(std::move(clique));
// }


//     // Step 4: KCore(G, |C*|)
//     const int32_t clique_lb = static_cast<int32_t>(g_incumbent_clique.size());
//     std::vector<int32_t> core = KCore(G, clique_lb);


//     // Step 5: DetermineSortOrder(G, core)
//     const SortOrder order = DetermineSortOrder(G, core);
//     // Convert core from G-labeling to H-labeling
//     std::vector<int32_t> core_H(order.relabel_to_original.size(), 0);
//     for (std::size_t vH = 0; vH < order.relabel_to_original.size(); ++vH) {
//     const int32_t vG = order.relabel_to_original[vH];
//     core_H[vH] = core[static_cast<std::size_t>(vG)];
// }


//     // Step 6: build LazyGraph H from G, order o, and core values.
//     LazyGraph H(G, order.relabel_to_original, order.original_to_relabel, core_H);

// {
//    // Step 7: CorenessBasedHeuristicSearch(H, core_H)

//     std::vector<int32_t> clique = coreness_based_heuristic(H, core_H);
//     try_update_incumbent(std::move(clique));


// }

//    // Step 8: SystematicSearch(H, core)
// {
//     std::vector<int32_t> clique = systematic_search(H, core_H);
//     try_update_incumbent(std::move(clique));
// }


//     // For now we simply return the current incumbent clique (possibly empty).
//     return g_incumbent_clique;
// }

// } // namespace lazymc
#include "lazymc/lazymc.h"
#include "lazymc/lazygraph.h"
#include "lazymc/kcore.h"
#include "lazymc/determine_sort_order.h"
#include "lazymc/degree_heuristic.h"
#include "lazymc/coreness_heuristic.h"
#include "lazymc/systematic_search.h"
#include "lazymc/incumbent.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <atomic>
#include <iostream> // NEW

namespace lazymc {

// Global incumbent clique, as in Algorithm 1 (C*).
std::vector<int32_t> g_incumbent_clique;
std::atomic<int32_t> g_best_size{0};

static void print_stage_done(const char* stage) {
    // assumes incumbent.h updates g_incumbent_clique (and/or g_best_size).
    // We print both; if you only trust one, you can remove the other.
    const int32_t best_vec = static_cast<int32_t>(g_incumbent_clique.size());
    const int32_t best_atomic = g_best_size.load(std::memory_order_relaxed);

    std::cout << "[LazyMC] done: " << stage
              << " | incumbent_size(vec)=" << best_vec
              << " | best_size(atomic)=" << best_atomic
              << std::endl;
}

std::vector<int32_t> LazyMC_run(const AdjList& G) {
    // Step 2: initialize incumbent clique C* = {}
    reset_incumbent();
    print_stage_done("reset_incumbent (Step 2)");

    // Step 3: DegreeBasedHeuristicSearch(G)
    {
        std::vector<int32_t> clique = degree_based_heuristic(G,3500);
        try_update_incumbent(std::move(clique));
        print_stage_done("degree_based_heuristic (Step 3)");
    }

    // Step 4: KCore(G, |C*|)
    const int32_t clique_lb = static_cast<int32_t>(g_incumbent_clique.size());
    std::vector<int32_t> core = KCore(G, clique_lb);
    print_stage_done("KCore (Step 4)");

    // Step 5: DetermineSortOrder(G, core)
    const SortOrder order = DetermineSortOrder(G, core);
    print_stage_done("DetermineSortOrder (Step 5)");

    // Convert core from G-labeling to H-labeling
    std::vector<int32_t> core_H(order.relabel_to_original.size(), 0);
    for (std::size_t vH = 0; vH < order.relabel_to_original.size(); ++vH) {
        const int32_t vG = order.relabel_to_original[vH];
        core_H[vH] = core[static_cast<std::size_t>(vG)];
    }
    print_stage_done("core_G_to_core_H conversion");

    // Step 6: build LazyGraph H from G, order o, and core values.
    LazyGraph H(G, order.relabel_to_original, order.original_to_relabel, core_H);
    print_stage_done("LazyGraph build (Step 6)");

    // Step 7: CorenessBasedHeuristicSearch(H, core_H)
    {
        std::vector<int32_t> clique = coreness_based_heuristic(H, core_H);
        try_update_incumbent(std::move(clique));
        print_stage_done("coreness_based_heuristic (Step 7)");
    }

    // Step 8: SystematicSearch(H, core_H)
    
    {
        std::vector<int32_t> clique = systematic_search(H, core_H);
        try_update_incumbent(std::move(clique));
        print_stage_done("systematic_search (Step 8)");
    }

    // Final
    print_stage_done("LazyMC_run done");
    return g_incumbent_clique;
}

} // namespace lazymc
