#include "lazymc/lazymc.h"
#include "lazymc/lazygraph.h"
#include "lazymc/kcore.h"
#include "lazymc/determine_sort_order.h"
#include "lazymc/degree_heuristic.h"
#include "lazymc/coreness_heuristic.h"
#include "lazymc/systematic_search.h"
#include "lazymc/incumbent.h"
#include "lazymc/profiling.h"
#include "lazymc/normalize.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace lazymc {

namespace {

// Number of highest-degree vertices tried by the degree-based heuristic.
constexpr int kDegreeHeuristicTopK = 120;

// Algorithm 1. Fills `profile` with per-stage statistics if it is given.
std::vector<int32_t> run_lazymc(const AdjList& input, ProfilingResult* profile) {
    const auto total_start = std::chrono::steady_clock::now();
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    Incumbent incumbent; // C*

    auto stage = [&](const char* name, unsigned threads, auto&& body) {
        StageStats stats;
        stats.name = name;
        stats.threads_used = threads;
        stats.clique_size_before = incumbent.size();
        {
            ScopedTimer timer(stats.duration_ms);
            body();
        }
        stats.clique_size_after = incumbent.size();
        if (profile) profile->stages.push_back(stats);
    };

    AdjList normalized;
    const AdjList* graph = &input;
    stage("NormalizeInput", 1, [&]() {
        if (!is_normalized(input)) {
            normalized = normalized_copy(input);
            graph = &normalized;
        }
    });
    const AdjList& G = *graph;
    const auto n = static_cast<int32_t>(G.size());

    // Step 3: DegreeBasedHeuristicSearch(G)
    stage("DegreeBasedHeuristic",
          std::min(hw, static_cast<unsigned>(std::min(kDegreeHeuristicTopK, n))), [&]() {
        incumbent.try_update(degree_based_heuristic(G, kDegreeHeuristicTopK));
    });

    // Step 4: KCore(G, |C*|). Only vertices of core >= |C*| can be in a
    // larger clique; all others get core 0 and are skipped from here on.
    std::vector<int32_t> core;
    stage("KCore", 1, [&]() { core = KCoreAtLeast(G, incumbent.size()); });

    // Step 5: DetermineSortOrder(G, core)
    SortOrder order;
    stage("DetermineSortOrder", 1, [&]() { order = DetermineSortOrder(G, core); });

    // Core values in H-labels.
    std::vector<int32_t> core_H(order.relabel_to_original.size(), 0);
    int32_t max_core = 0;
    for (std::size_t vH = 0; vH < order.relabel_to_original.size(); ++vH) {
        const int32_t vG = order.relabel_to_original[vH];
        core_H[vH] = core[static_cast<std::size_t>(vG)];
        max_core = std::max(max_core, core_H[vH]);
    }

    // Step 6: build LazyGraph H from G, order o, and core values.
    LazyGraph H(G, order.relabel_to_original, order.original_to_relabel, core_H, &incumbent);

    // Step 7: CorenessBasedHeuristicSearch(H, core_H)
    stage("CorenessBasedHeuristic", std::min(hw, static_cast<unsigned>(max_core)), [&]() {
        coreness_based_heuristic(H, core_H, incumbent);
    });

    // Step 8: SystematicSearch(H, core_H)
    stage("SystematicSearch", hw, [&]() {
        systematic_search(H, core_H, incumbent);
    });

    std::vector<int32_t> clique = incumbent.clique();
    if (profile) {
        profile->total_time_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - total_start).count();
        profile->final_clique_size = static_cast<int32_t>(clique.size());
        profile->final_clique = clique;
    }
    return clique;
}

} // namespace

std::vector<int32_t> LazyMC_run(const AdjList& G) {
    return run_lazymc(G, nullptr);
}

ProfilingResult LazyMC_run_profiled(const AdjList& G) {
    ProfilingResult result;
    run_lazymc(G, &result);
    return result;
}

} // namespace lazymc
