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
#include "lazymc/profiling.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>
#include <thread>

namespace lazymc {

// Global incumbent clique, as in Algorithm 1 (C*).
std::shared_ptr<const std::vector<int32_t>> g_incumbent_clique =
    std::make_shared<const std::vector<int32_t>>();
std::atomic<int32_t> g_best_size{0};

std::vector<int32_t> LazyMC_run(const AdjList& G) {
    // Step 2: initialize incumbent clique C* = {}
    reset_incumbent();

    // Step 3: DegreeBasedHeuristicSearch(G)
    {
        std::vector<int32_t> clique = degree_based_heuristic(G, 120);
        try_update_incumbent(std::move(clique));
    }

    // Step 4: KCore(G, |C*|)
    const auto incumbent_ptr =
        std::atomic_load_explicit(&g_incumbent_clique,
                                  std::memory_order_acquire);
    const int32_t clique_lb =
        incumbent_ptr ? static_cast<int32_t>(incumbent_ptr->size()) : 0;
    std::vector<int32_t> core = KCore(G, clique_lb);

    // Step 5: DetermineSortOrder(G, core)
    SortOrder order = DetermineSortOrder(G, core);

    // Convert core from G-labeling to H-labeling
    std::vector<int32_t> core_H(order.relabel_to_original.size(), 0);
    for (std::size_t vH = 0; vH < order.relabel_to_original.size(); ++vH) {
        const int32_t vG = order.relabel_to_original[vH];
        core_H[vH] = core[static_cast<std::size_t>(vG)];
    }

    // Step 6: build LazyGraph H from G, order o, and core values.
    LazyGraph H(G, order.relabel_to_original, order.original_to_relabel, core_H);

    // Step 7: CorenessBasedHeuristicSearch(H, core_H)
    {
        std::vector<int32_t> clique = coreness_based_heuristic(H, core_H);
        try_update_incumbent(std::move(clique));
    }

    // Step 8: SystematicSearch(H, core_H)
    
    {
        std::vector<int32_t> clique = systematic_search(H, core_H);
        try_update_incumbent(std::move(clique));
    }

    // Final
    const auto final_incumbent =
        std::atomic_load_explicit(&g_incumbent_clique,
                                  std::memory_order_acquire);
    if (final_incumbent) return *final_incumbent;
    return {};
}

// Profiled version: collects timing and clique info for each stage
ProfilingResult LazyMC_run_profiled(const AdjList& G) {
    ProfilingResult result;
    auto total_start = std::chrono::high_resolution_clock::now();
    
    // Step 2: initialize incumbent clique C* = {}
    reset_incumbent();
    
    const int32_t n = static_cast<int32_t>(G.size());
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    // ========== Stage 1: Degree-Based Heuristic Search ==========
    {
        StageStats stats;
        stats.name = "DegreeBasedHeuristic";
        stats.clique_size_before = g_best_size.load(std::memory_order_relaxed);
        
        // Compute thread count: min(hw, top_k, n)
        const int top_k = 120;
        stats.threads_used = std::min(hw, static_cast<unsigned>(std::min(top_k, n)));
        
        {
            ScopedTimer timer(stats.duration_ms);
            std::vector<int32_t> clique = degree_based_heuristic(G, top_k);
            try_update_incumbent(std::move(clique));
        }
        
        stats.clique_size_after = g_best_size.load(std::memory_order_relaxed);
        result.stages.push_back(stats);
    }

    // ========== Stage 2: KCore Decomposition ==========
    std::vector<int32_t> core;
    {
        StageStats stats;
        stats.name = "KCore";
        stats.clique_size_before = g_best_size.load(std::memory_order_relaxed);
        stats.threads_used = 1;  // KCore is sequential
        
        {
            ScopedTimer timer(stats.duration_ms);
            const auto incumbent_ptr =
                std::atomic_load_explicit(&g_incumbent_clique,
                                          std::memory_order_acquire);
            const int32_t clique_lb =
                incumbent_ptr ? static_cast<int32_t>(incumbent_ptr->size()) : 0;
            core = KCore(G, clique_lb);
        }
        
        stats.clique_size_after = g_best_size.load(std::memory_order_relaxed);
        result.stages.push_back(stats);
    }

    // ========== Stage 3: Determine Sort Order ==========
    SortOrder order;
    {
        StageStats stats;
        stats.name = "DetermineSortOrder";
        stats.clique_size_before = g_best_size.load(std::memory_order_relaxed);
        stats.threads_used = 1;  // Sequential
        
        {
            ScopedTimer timer(stats.duration_ms);
            order = DetermineSortOrder(G, core);
        }
        
        stats.clique_size_after = g_best_size.load(std::memory_order_relaxed);
        result.stages.push_back(stats);
    }

    // Convert core from G-labeling to H-labeling
    std::vector<int32_t> core_H(order.relabel_to_original.size(), 0);
    for (std::size_t vH = 0; vH < order.relabel_to_original.size(); ++vH) {
        const int32_t vG = order.relabel_to_original[vH];
        core_H[vH] = core[static_cast<std::size_t>(vG)];
    }

    // ========== Stage 4: Build LazyGraph ==========
    LazyGraph H(G, order.relabel_to_original, order.original_to_relabel, core_H);

    // ========== Stage 5: Coreness-Based Heuristic Search ==========
    {
        StageStats stats;
        stats.name = "CorenessBasedHeuristic";
        stats.clique_size_before = g_best_size.load(std::memory_order_relaxed);
        
        // Compute thread count: based on number of unique core values
        int32_t max_core = 0;
        for (auto c : core_H) {
            if (c > max_core) max_core = c;
        }
        stats.threads_used = std::min(hw, static_cast<unsigned>(max_core));
        
        {
            ScopedTimer timer(stats.duration_ms);
            std::vector<int32_t> clique = coreness_based_heuristic(H, core_H);
            try_update_incumbent(std::move(clique));
        }
        
        stats.clique_size_after = g_best_size.load(std::memory_order_relaxed);
        result.stages.push_back(stats);
    }

    // ========== Stage 6: Systematic Search ==========
    {
        StageStats stats;
        stats.name = "SystematicSearch";
        stats.clique_size_before = g_best_size.load(std::memory_order_relaxed);
        stats.threads_used = hw;  // Assumes parallel systematic search
        
        {
            ScopedTimer timer(stats.duration_ms);
            std::vector<int32_t> clique = systematic_search(H, core_H);
            try_update_incumbent(std::move(clique));
        }
        
        stats.clique_size_after = g_best_size.load(std::memory_order_relaxed);
        result.stages.push_back(stats);
    }

    // Finalize result
    auto total_end = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
    const auto final_incumbent =
        std::atomic_load_explicit(&g_incumbent_clique,
                                  std::memory_order_acquire);
    result.final_clique_size =
        final_incumbent ? static_cast<int32_t>(final_incumbent->size()) : 0;
    if (final_incumbent) {
        result.final_clique = *final_incumbent;
    }
    
    return result;
}

} // namespace lazymc
