#include "lazymc/degree_heuristic.h"
#include "lazymc/intersect_functions.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lazymc {

namespace {

struct GlobalIncumbent {
    std::atomic<int32_t> best_size{0};
    mutable std::shared_mutex mtx;
    std::vector<int32_t> best;

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

static void run_candidate(LazyGraph& H, int32_t v, GlobalIncumbent& global) {
    std::vector<int32_t> tmp;

    std::vector<int32_t> N;
    const auto& Nv = H.sorted_neighbors(v);
    N.reserve(Nv.size());

    // Algorithm 5, line 4: prune by global incumbent size |C*|
    const int32_t incumbent_sz = global.size();
    for (int32_t u : Nv) {
        if (static_cast<int32_t>(H.sorted_neighbors(u).size()) >= incumbent_sz) {
            N.push_back(u);
        }
    }

    std::vector<int32_t> C;
    C.reserve(64);
    C.push_back(v);

    while (!N.empty()) {
        int32_t best_u = -1;
        int32_t max_intersection = -1;

        for (int32_t w : N) {
            const auto& Bw = H.hashed_neighbors(w);
            int32_t d = intersect_gt(N, Bw, tmp, max_intersection);
            if (d > max_intersection) {
                max_intersection = d;
                best_u = w;
            }
        }

        if (best_u < 0) break;

        C.push_back(best_u);

        const auto& Bu = H.hashed_neighbors(best_u);
        int32_t d = intersect_gt(N, Bu, tmp, 0);

        if (d <= 0) {
            N.clear();
        } else {
            N.assign(tmp.begin(), tmp.begin() + d);
            N.erase(std::remove(N.begin(), N.end(), best_u), N.end());
        }
    }

    global.try_update(std::move(C));
}

} // namespace

std::vector<int32_t> degree_based_heuristic(const AdjList& G, int top_k) {
    if (G.empty()) return {};

    const int32_t n = static_cast<int32_t>(G.size());

    // Identity relabeling: H-ids == G-ids.
    std::vector<int32_t> relabel_to_original(n);
    std::vector<int32_t> original_to_relabel(n);
    std::vector<int32_t> core(n, 0);

    for (int32_t i = 0; i < n; ++i) {
        relabel_to_original[i] = i;
        original_to_relabel[i] = i;
    }

    LazyGraph H(G, relabel_to_original, original_to_relabel, core);
    return degree_based_heuristic(H, top_k);
}

std::vector<int32_t> degree_based_heuristic(LazyGraph& H, int top_k) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return {};

    std::vector<std::pair<int32_t, int32_t>> degree_vertex;
    degree_vertex.reserve(static_cast<std::size_t>(n));

    for (int32_t v = 0; v < n; ++v) {
        degree_vertex.push_back({static_cast<int32_t>(H.sorted_neighbors(v).size()), v});
    }

    std::sort(degree_vertex.begin(), degree_vertex.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    const int32_t k = std::min<int32_t>(top_k, n);

    GlobalIncumbent global;

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workers = std::min<unsigned>(hw, static_cast<unsigned>(k));

    std::atomic<int32_t> next{0};
    std::vector<std::thread> pool;
    pool.reserve(workers);

    for (unsigned t = 0; t < workers; ++t) {
        pool.emplace_back([&]() {
            while (true) {
                const int32_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= k) break;

                const int32_t v = degree_vertex[static_cast<std::size_t>(i)].second;
                run_candidate(H, v, global);
            }
        });
    }

    for (auto& th : pool) {
        th.join();
    }

    std::shared_lock<std::shared_mutex> lock(global.mtx);
    return global.best;
}

} // namespace lazymc
