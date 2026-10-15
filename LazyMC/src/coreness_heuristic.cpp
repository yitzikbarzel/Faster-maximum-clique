#include "lazymc/coreness_heuristic.h"
#include "lazymc/intersect_functions.h"
#include "lazymc/incumbent.h"


#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lazymc {

namespace {

// Greedy clique growth from v (Algorithm 6, one start vertex).
void run_candidate_core(LazyGraph& H, int32_t v, Incumbent& incumbent) {
    std::vector<int32_t> tmp;

    std::vector<int32_t> N;
    const auto& Nv = H.sorted_neighbors(v);
    N.assign(std::upper_bound(Nv.begin(), Nv.end(), v), Nv.end());

    std::vector<int32_t> C;
    C.reserve(64);
    C.push_back(v);

    while (!N.empty()) {
        const int32_t u = *std::max_element(N.begin(), N.end());

        // θ = |C*| − |C|  (global, grows as other threads find cliques)
        const int32_t theta = incumbent.size() - static_cast<int32_t>(C.size());

        const auto& Bu = H.hashed_neighbors(u);
        const int32_t d = intersect_gt(N, Bu, tmp, theta);

        C.push_back(u);

        if (d <= 0) {
            break;
        }

        N.assign(tmp.begin(), tmp.begin() + d);
        N.erase(std::remove(N.begin(), N.end(), u), N.end());
    }

    if (static_cast<int32_t>(C.size()) <= incumbent.size()) return;
    for (int32_t& x : C) x = H.to_original(x);
    incumbent.try_update(std::move(C));
}

} // namespace

// Parallel coreness-based heuristic search (Algorithm 6 style)
std::vector<int32_t> coreness_based_heuristic(LazyGraph& H,
                                             const std::vector<int32_t>& core,
                                             Incumbent& incumbent) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return incumbent.clique();

    // One start vertex per k: the lowest-numbered vertex with core[v] == k.
    int32_t max_core = 0;
    for (int32_t v = 0; v < n; ++v) {
        max_core = std::max(max_core, core[static_cast<std::size_t>(v)]);
    }
    if (max_core <= 0) return incumbent.clique();

    std::vector<int32_t> first(static_cast<std::size_t>(max_core) + 1, -1);
    for (int32_t v = n; v-- > 0;) {
        const int32_t cv = core[static_cast<std::size_t>(v)];
        if (cv >= 1) first[static_cast<std::size_t>(cv)] = v;
    }

    std::vector<int32_t> starts;
    for (int32_t k = max_core; k >= 1; --k) {
        if (first[static_cast<std::size_t>(k)] >= 0) starts.push_back(first[static_cast<std::size_t>(k)]);
    }

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
                run_candidate_core(H, starts[static_cast<std::size_t>(i)], incumbent);
            }
        });
    }

    for (auto& th : pool) {
        th.join();
    }

    return incumbent.clique();
}

std::vector<int32_t> coreness_based_heuristic(LazyGraph& H,
                                             const std::vector<int32_t>& core) {
    Incumbent incumbent;
    return coreness_based_heuristic(H, core, incumbent);
}

} // namespace lazymc
