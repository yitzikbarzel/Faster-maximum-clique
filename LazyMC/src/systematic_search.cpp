#include "lazymc/systematic_search.h"
#include "lazymc/neighbor_search.h"
#include "lazymc/incumbent.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace lazymc {

namespace {

struct SharedIncumbent {
    std::atomic<int32_t> best_size_atomic;
    mutable std::shared_mutex best_mutex;
    std::vector<int32_t> best_clique; // Stored in G-labeling

    SharedIncumbent() : best_size_atomic(0) {
        sync_incumbent_size();
        best_size_atomic.store(
            g_best_size.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        const auto incumbent =
            std::atomic_load_explicit(&g_incumbent_clique,
                                      std::memory_order_acquire);
        if (incumbent) {
            best_clique = *incumbent;
        }
    }

    int32_t best_size() const noexcept {
        return best_size_atomic.load(std::memory_order_relaxed);
    }

    std::vector<int32_t> snapshot_best_clique() const {
        std::shared_lock<std::shared_mutex> lock(best_mutex);
        return best_clique;
    }

    void try_update_best_clique(std::vector<int32_t>&& candidate_clique) {
        const int32_t candidate_size = static_cast<int32_t>(candidate_clique.size());
        if (candidate_size <= best_size()) return;

        std::unique_lock<std::shared_mutex> lock(best_mutex);
        if (candidate_size > static_cast<int32_t>(best_clique.size())) {
            best_clique = std::move(candidate_clique);
            best_size_atomic.store(
                static_cast<int32_t>(best_clique.size()),
                std::memory_order_relaxed
            );
        }
    }
};

// ------------------------------------------------------------
// Cheap bound: can `vertex` possibly improve the incumbent?
// Uses only degree + core_H (already computed).
// ------------------------------------------------------------
inline bool can_improve_incumbent(SharedIncumbent& incumbent,
                                 LazyGraph& graph,
                                 int32_t vertex,
                                 const std::vector<int32_t>& core_H) {
    const int32_t incumbent_size = incumbent.best_size();
    // if (incumbent_size <= 1) return true;

    const auto& neighbors_v = graph.sorted_neighbors(vertex);

    // To beat clique size = incumbent_size, we need at least incumbent_size
    // candidates in the right-neighborhood (because clique includes `vertex`).
    const int32_t required = incumbent_size;

    // Degree bound: must have more than `required` total neighbors available.
    if (static_cast<int32_t>(neighbors_v.size()) < required) return false;

    // Right-neighborhood bound: need `required` neighbors u > vertex with core[u] >= incumbent_size.
    int32_t eligible_right_neighbors = 0;
    for (int32_t u : neighbors_v) {
        if (u > vertex && core_H[static_cast<std::size_t>(u)] >= incumbent_size) {
            if (++eligible_right_neighbors >= required) return true;
        }
    }
    return false;
}

} // namespace

std::vector<int32_t> systematic_search(LazyGraph& graph,
                                       const std::vector<int32_t>& core_H) {
    const int32_t num_vertices = static_cast<int32_t>(graph.num_vertices());
    if (num_vertices <= 0) return {};

    // max_core = maximum core value
    int32_t max_core = 0;
    for (int32_t v = 0; v < num_vertices; ++v) {
        max_core = std::max(max_core, core_H[static_cast<std::size_t>(v)]);
    }
    if (max_core <= 0) return {};

    // Upper bound: omega(G) <= max_core + 1
    const int32_t omega_upper_bound = max_core + 1;

    // vertices_by_core[k] = { v : core_H[v] == k }
    std::vector<std::vector<int32_t>> vertices_by_core(static_cast<std::size_t>(max_core + 1));
    for (int32_t v = 0; v < num_vertices; ++v) {
        const int32_t k = core_H[static_cast<std::size_t>(v)];
        if (k >= 0 && k <= max_core) {
            vertices_by_core[static_cast<std::size_t>(k)].push_back(v);
        }
    }

    constexpr double density_threshold_phi = 0.7;
    SharedIncumbent incumbent;

    const unsigned hardware_threads =
        std::max(1u, std::thread::hardware_concurrency());

    // ------------------------------------------------------------
    // Lines 2–5: parfor k from |C*| to max_core
    // For each k, pick the lowest-numbered vertex with core==k.
    // ------------------------------------------------------------
    {
        std::atomic<int32_t> next_core_k{incumbent.best_size()};
        std::vector<std::thread> threads;
        threads.reserve(hardware_threads);

        for (unsigned thread_id = 0; thread_id < hardware_threads; ++thread_id) {
            threads.emplace_back([&]() {
                std::vector<int32_t> candidate; // Reused inside this worker thread.

                while (true) {
                    const int32_t k =
                        next_core_k.fetch_add(1, std::memory_order_relaxed);
                    if (k > max_core) break;

                    const int32_t inc_size = incumbent.best_size();
                    if (inc_size >= omega_upper_bound) break;
                    if (k < inc_size) continue;

                    const auto& bucket_vertices =
                        vertices_by_core[static_cast<std::size_t>(k)];
                    if (bucket_vertices.empty()) continue;

                    const int32_t seed_vertex = bucket_vertices.front(); // lowest-numbered
                    if (!can_improve_incumbent(incumbent, graph, seed_vertex, core_H)) continue;

                    candidate.clear();
                    if (NeighborSearch(graph, seed_vertex, core_H, inc_size, density_threshold_phi, candidate)) {
                        incumbent.try_update_best_clique(std::move(candidate));
                        candidate.clear();
                    }
                }
            });
        }

        for (auto& th : threads) th.join();
    }

    if (incumbent.best_size() >= omega_upper_bound) {
        return incumbent.snapshot_best_clique();
    }

    // ------------------------------------------------------------
    // Lines 6–11: for k desc, parfor v with core==k
    // ------------------------------------------------------------
    for (int32_t k = max_core; k >= 1; --k) {
        if (k < incumbent.best_size()) break;

        const auto& bucket_vertices =
            vertices_by_core[static_cast<std::size_t>(k)];
        if (bucket_vertices.empty()) continue;

        const unsigned num_workers =
            std::min<unsigned>(hardware_threads,
                               static_cast<unsigned>(bucket_vertices.size()));
        std::atomic<std::size_t> next_index{0};

        std::vector<std::thread> threads;
        threads.reserve(num_workers);

        for (unsigned worker_id = 0; worker_id < num_workers; ++worker_id) {
            threads.emplace_back([&]() {
                std::vector<int32_t> candidate; // Reused inside this worker thread.

                while (true) {
                    const int32_t inc_size = incumbent.best_size();
                    if (inc_size >= omega_upper_bound) break;
                    if (k < inc_size) break;

                    const std::size_t i = next_index.fetch_add(1, std::memory_order_relaxed);
                    if (i >= bucket_vertices.size()) break;

                    const int32_t vertex = bucket_vertices[i];
                    if (!can_improve_incumbent(incumbent, graph, vertex, core_H)) continue;

                    candidate.clear();
                    if (NeighborSearch(graph, vertex, core_H, inc_size, density_threshold_phi, candidate)) {
                        incumbent.try_update_best_clique(std::move(candidate));
                        candidate.clear();
                    }
                }
            });
        }

        for (auto& th : threads) th.join();
    }

    return incumbent.snapshot_best_clique();
}

} // namespace lazymc
