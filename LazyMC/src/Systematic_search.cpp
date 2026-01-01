#include "lazymc/systematic_search.h"
#include "lazymc/neighbor_search.h"
#include "lazymc/incumbent.h"


#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace lazymc {

namespace {

struct GlobalIncumbent {
    std::atomic<int32_t> best_size;
    mutable std::shared_mutex mtx;
    std::vector<int32_t> best; b
    GlobalIncumbent() : best_size(0) {
        sync_incumbent_size();
        best_size.store(g_best_size.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
        best = g_incumbent_clique; 
    }

    int32_t size() const noexcept { return best_size.load(std::memory_order_relaxed); }

    std::vector<int32_t> copy_best() const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return best;
    }

    void try_update(std::vector<int32_t>&& candidate) {
        const int32_t s = static_cast<int32_t>(candidate.size());
        if (s <= size()) return;

        std::unique_lock<std::shared_mutex> lock(mtx);
        if (s > static_cast<int32_t>(best.size())) {
            best = std::move(candidate);
            best_size.store(static_cast<int32_t>(best.size()), std::memory_order_relaxed);
        }
    }
};


// ------------------------------------------------------------
// Cheap bound: can v possibly beat current incumbent?
// Uses only degree + core_H (already computed).
// ------------------------------------------------------------
inline bool can_beat(GlobalIncumbent& global,
                     LazyGraph& H,
                     int32_t v,
                     const std::vector<int32_t>& core_H) {
    const int32_t inc = global.size();
    if (inc <= 1) return true;

    const auto& Nv = H.sorted_neighbors(v);

    // Degree bound: need at least inc-1 neighbors
    if (static_cast<int32_t>(Nv.size()) < inc - 1) return false;

    // Right-neighborhood bound: need inc-1 neighbors u>v with core[u] >= inc
    int32_t cnt = 0;
    for (int32_t u : Nv) {
        if (u > v && core_H[static_cast<std::size_t>(u)] >= inc) {
            if (++cnt >= inc - 1) return true;
        }
    }
    return false;
}

} // namespace

std::vector<int32_t> systematic_search(LazyGraph& H,
                                       const std::vector<int32_t>& core_H) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return {};

    // degeneracy = max core value
    int32_t degeneracy = 0;
    for (int32_t v = 0; v < n; ++v) {
        degeneracy = std::max(degeneracy, core_H[static_cast<std::size_t>(v)]);
    }
    if (degeneracy <= 0) return {};

    // Upper bound: omega(G) <= degeneracy + 1
    const int32_t ub = degeneracy + 1;

    // buckets[k] = { v : core_H[v] == k }
    std::vector<std::vector<int32_t>> buckets(static_cast<std::size_t>(degeneracy + 1));
    for (int32_t v = 0; v < n; ++v) {
        const int32_t k = core_H[static_cast<std::size_t>(v)];
        if (k >= 0 && k <= degeneracy) {
            buckets[static_cast<std::size_t>(k)].push_back(v);
        }
    }

    constexpr double phi = 0.7;
    GlobalIncumbent global;

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    // ------------------------------------------------------------
    // Lines 2–5: parfor k from |C*| to degeneracy
    // ------------------------------------------------------------
    {
        std::atomic<int32_t> next_k{global.size()};
        std::vector<std::thread> pool;
        pool.reserve(hw);

        for (unsigned t = 0; t < hw; ++t) {
            pool.emplace_back([&]() {
                while (true) {
                    const int32_t k = next_k.fetch_add(1, std::memory_order_relaxed);
                    if (k > degeneracy) break;

                    const int32_t inc = global.size();
                    if (inc >= ub) break;
                    if (k < inc) continue;

                    const auto& verts = buckets[static_cast<std::size_t>(k)];
                    if (verts.empty()) continue;

                    const int32_t v0 = verts.front(); // lowest-numbered
                    if (!can_beat(global, H, v0, core_H)) continue;

                    std::vector<int32_t> local = global.copy_best();
                    NeighborSearch(H, v0, core_H, local, phi);
                    global.try_update(std::move(local));
                }
            });
        }

        for (auto& th : pool) th.join();
    }

    if (global.size() >= ub) {
        return global.copy_best();
    }

    // ------------------------------------------------------------
    // Lines 6–11: for k desc, parfor v with core==k
    // ------------------------------------------------------------
    for (int32_t k = degeneracy; k >= 1; --k) {
        if (k < global.size()) continue;

        const auto& verts = buckets[static_cast<std::size_t>(k)];
        if (verts.empty()) continue;

        const unsigned workers =
            std::min<unsigned>(hw, static_cast<unsigned>(verts.size()));
        std::atomic<std::size_t> next{0};

        std::vector<std::thread> pool;
        pool.reserve(workers);

        for (unsigned t = 0; t < workers; ++t) {
            pool.emplace_back([&]() {
                std::vector<int32_t> local = global.copy_best();
                int32_t seen = static_cast<int32_t>(local.size());

                while (true) {
                    const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
                    if (i >= verts.size()) break;

                    const int32_t inc = global.size();
                    if (inc >= ub) break;
                    if (k < inc) break;

                    if (inc > seen) {
                        local = global.copy_best();
                        seen = inc;
                    }

                    const int32_t v = verts[i];
                    if (!can_beat(global, H, v, core_H)) continue;

                    NeighborSearch(H, v, core_H, local, phi);

                    const int32_t now = static_cast<int32_t>(local.size());
                    if (now > inc) {
                        global.try_update(std::vector<int32_t>(local));
                        seen = now;
                    }
                }

                global.try_update(std::move(local));
            });
        }

        for (auto& th : pool) th.join();
    }

    return global.copy_best();
}

} // namespace lazymc
