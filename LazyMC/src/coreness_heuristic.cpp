#include "lazymc/coreness_heuristic.h"
#include "lazymc/intersect_functions.h"

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
    std::atomic<int32_t> best_size{0};
    mutable std::shared_mutex mtx;
    std::vector<int32_t> best; // Stored in H-labeling

    int32_t size() const noexcept {
        return best_size.load(std::memory_order_relaxed);
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

static void run_candidate_core(
    LazyGraph& H,
    int32_t v,
    GlobalIncumbent& global
) {
    std::vector<int32_t> tmp;

    // N <- N_G^+(v)
    std::vector<int32_t> N;
    const auto& Nv = H.sorted_neighbors(v);
    N.reserve(Nv.size());
    for (int32_t u : Nv) {
        if (u > v) N.push_back(u);
    }

    // C = {v}
    std::vector<int32_t> C;
    C.reserve(64);
    C.push_back(v);

    while (!N.empty()) {
        // u <- highest-numbered vertex in N
        const int32_t u = *std::max_element(N.begin(), N.end());

        // theta = |C*| - |C|
        const int32_t theta =
            global.size() - static_cast<int32_t>(C.size());

        // N <- N ∩ N_G(u)   (intersect-gt with threshold theta)
        const auto& Bu = H.hashed_neighbors(u);
        const int32_t d = intersect_gt(N, Bu, tmp, theta);

        // C <- C ∪ {u}
        C.push_back(u);

        if (d <= 0) {
            N.clear();
        } else {
            N.assign(tmp.begin(), tmp.begin() + d);
            N.erase(std::remove(N.begin(), N.end(), u), N.end());
        }
    }

    global.try_update(std::move(C));
}

} // namespace

// Parallel coreness-based heuristic search (Algorithm 6 style)
std::vector<int32_t> coreness_based_heuristic(
    LazyGraph& H,
    const std::vector<int32_t>& core
) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return {};

    // Compute max core value
    int32_t max_core = 0;
    for (int32_t v = 0; v < n; ++v) {
        const int32_t cv = core[static_cast<std::size_t>(v)];
        if (cv > max_core) max_core = cv;
    }
    if (max_core <= 0) return {};

    // Precompute one start vertex v per k: lowest-numbered vertex with core[v] == k
    std::vector<int32_t> starts;
    starts.reserve(static_cast<std::size_t>(max_core));

    for (int32_t k = max_core; k >= 1; --k) {
        int32_t v = -1;
        for (int32_t i = 0; i < n; ++i) {
            if (core[static_cast<std::size_t>(i)] == k) {
                v = i;
                break;
            }
        }
        if (v >= 0) starts.push_back(v);
    }

    if (starts.empty()) return {};

    GlobalIncumbent global;

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workers = std::min<unsigned>(hw, static_cast<unsigned>(starts.size()));

    std::atomic<int32_t> next{0};
    std::vector<std::thread> pool;
    pool.reserve(workers);

    for (unsigned t = 0; t < workers; ++t) {
        pool.emplace_back([&]() {
            while (true) {
                const int32_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= static_cast<int32_t>(starts.size())) break;

                const int32_t v = starts[static_cast<std::size_t>(i)];
                run_candidate_core(H, v, global);
            }
        });
    }

    for (auto& th : pool) {
        th.join();
    }

    // Convert best clique from H-labeling to G-labeling
    std::vector<int32_t> best_H;
    {
        std::shared_lock<std::shared_mutex> lock(global.mtx);
        best_H = global.best;
    }

    std::vector<int32_t> best_G;
    best_G.reserve(best_H.size());
    for (int32_t vH : best_H) {
        best_G.push_back(H.to_original(vH));
    }
    return best_G;
}

} // namespace lazymc
