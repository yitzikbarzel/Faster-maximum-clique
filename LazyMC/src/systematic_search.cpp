#include "lazymc/systematic_search.h"
#include "lazymc/neighbor_search.h"
#include "lazymc/incumbent.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace lazymc {

namespace {

constexpr double density_threshold_phi = 0.7;

// ------------------------------------------------------------
// Cheap bound: can `vertex` possibly improve the incumbent?
// Uses only degree + core_H (already computed).
// ------------------------------------------------------------
inline bool can_improve_incumbent(int32_t incumbent_size,
                                  LazyGraph& graph,
                                  int32_t vertex,
                                  const std::vector<int32_t>& core_H) {
    const auto& neighbors_v = graph.sorted_neighbors(vertex);

    // To beat clique size = incumbent_size, we need at least incumbent_size
    // candidates in the right-neighborhood (because clique includes `vertex`).
    const int32_t required = incumbent_size;
    if (required <= 0) return true;
    if (static_cast<int32_t>(neighbors_v.size()) < required) return false;

    // Right-neighborhood bound: need `required` neighbors u > vertex with core[u] >= incumbent_size.
    int32_t eligible_right_neighbors = 0;
    for (auto it = std::upper_bound(neighbors_v.begin(), neighbors_v.end(), vertex);
         it != neighbors_v.end(); ++it) {
        if (core_H[static_cast<std::size_t>(*it)] >= incumbent_size) {
            if (++eligible_right_neighbors >= required) return true;
        }
    }
    return false;
}

template <class Body>
void run_workers(unsigned num_workers, Body&& body) {
    std::vector<std::thread> threads;
    threads.reserve(num_workers);
    for (unsigned t = 0; t < num_workers; ++t) threads.emplace_back(body);
    for (auto& th : threads) th.join();
}

void search_vertex(LazyGraph& graph,
                   int32_t vertex,
                   const std::vector<int32_t>& core_H,
                   Incumbent& incumbent,
                   std::vector<int32_t>& candidate) {
    if (!can_improve_incumbent(incumbent.size(), graph, vertex, core_H)) return;

    candidate.clear();
    if (NeighborSearch(graph, vertex, core_H, incumbent, density_threshold_phi, candidate)) {
        incumbent.try_update(std::move(candidate));
        candidate.clear();
    }
}

} // namespace

std::vector<int32_t> systematic_search(LazyGraph& graph,
                                       const std::vector<int32_t>& core_H,
                                       Incumbent& incumbent) {
    const int32_t num_vertices = static_cast<int32_t>(graph.num_vertices());
    if (num_vertices <= 0) return incumbent.clique();

    int32_t max_core = 0;
    for (int32_t v = 0; v < num_vertices; ++v) {
        max_core = std::max(max_core, core_H[static_cast<std::size_t>(v)]);
    }
    if (max_core <= 0) return incumbent.clique();

    // Upper bound: omega(G) <= max_core + 1
    const int32_t omega_upper_bound = max_core + 1;

    // Vertices with core >= 1, ordered by core descending, ties by id.
    std::vector<int32_t> by_core;
    std::vector<int32_t> first_of_core(static_cast<std::size_t>(max_core) + 1, -1);
    {
        std::vector<int32_t> count(static_cast<std::size_t>(max_core) + 2, 0);
        for (int32_t v = 0; v < num_vertices; ++v) {
            const int32_t k = core_H[static_cast<std::size_t>(v)];
            if (k >= 1) ++count[static_cast<std::size_t>(max_core - k)];
        }
        std::vector<int32_t> start(count.size(), 0);
        for (std::size_t i = 1; i < count.size(); ++i) start[i] = start[i - 1] + count[i - 1];
        by_core.resize(static_cast<std::size_t>(start.back() + count.back()));
        for (int32_t v = 0; v < num_vertices; ++v) {
            const int32_t k = core_H[static_cast<std::size_t>(v)];
            if (k < 1) continue;
            if (first_of_core[static_cast<std::size_t>(k)] < 0) first_of_core[static_cast<std::size_t>(k)] = v;
            by_core[static_cast<std::size_t>(start[static_cast<std::size_t>(max_core - k)]++)] = v;
        }
    }

    const unsigned hardware_threads = std::max(1u, std::thread::hardware_concurrency());

    // ------------------------------------------------------------
    // Lines 2–5: parfor k from |C*| to max_core
    // For each k, search the lowest-numbered vertex with core == k.
    // A searched vertex need not be searched again in lines 6–11: its search
    // already ruled out anything larger than the incumbent at that time, and
    // the incumbent only grows.
    // ------------------------------------------------------------
    std::vector<std::uint8_t> searched(static_cast<std::size_t>(num_vertices), 0);
    {
        std::atomic<int32_t> next_core_k{std::max(incumbent.size(), 1)};
        run_workers(hardware_threads, [&]() {
            std::vector<int32_t> candidate;
            while (true) {
                const int32_t k = next_core_k.fetch_add(1, std::memory_order_relaxed);
                if (k > max_core) break;
                if (incumbent.size() >= omega_upper_bound) break;
                if (k < incumbent.size()) continue;

                const int32_t seed = first_of_core[static_cast<std::size_t>(k)];
                if (seed >= 0) {
                    search_vertex(graph, seed, core_H, incumbent, candidate);
                    searched[static_cast<std::size_t>(seed)] = 1;
                }
            }
        });
    }

    // ------------------------------------------------------------
    // Lines 6–11: for k desc, parfor v with core == k.
    // Vertices are handed out in core-descending order; once a vertex's core
    // is below |C*| so are all later ones, and the search is done.
    // ------------------------------------------------------------
    if (incumbent.size() < omega_upper_bound) {
        std::atomic<std::size_t> next_index{0};
        const unsigned num_workers = std::min<unsigned>(
            hardware_threads, static_cast<unsigned>(std::max<std::size_t>(by_core.size(), 1)));
        run_workers(num_workers, [&]() {
            std::vector<int32_t> candidate;
            while (true) {
                const int32_t inc_size = incumbent.size();
                if (inc_size >= omega_upper_bound) break;

                const std::size_t i = next_index.fetch_add(1, std::memory_order_relaxed);
                if (i >= by_core.size()) break;

                const int32_t vertex = by_core[i];
                if (core_H[static_cast<std::size_t>(vertex)] < inc_size) break;
                if (searched[static_cast<std::size_t>(vertex)]) continue;

                search_vertex(graph, vertex, core_H, incumbent, candidate);
            }
        });
    }

    return incumbent.clique();
}

std::vector<int32_t> systematic_search(LazyGraph& graph,
                                       const std::vector<int32_t>& core_H) {
    Incumbent incumbent;
    return systematic_search(graph, core_H, incumbent);
}

} // namespace lazymc
