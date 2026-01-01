#include "lazymc/degree_heuristic.h"
#include "lazymc/intersect_functions.h"
#include "lazymc/lazygraph.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>
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
            best_size.store(static_cast<int32_t>(best.size()),
                            std::memory_order_relaxed);
        }
    }
};

// Algorithm 5 worker for one v (H-labeling here is identity in wrapper below).
static void run_candidate(LazyGraph& H,
                          int32_t v,
                          const std::vector<int32_t>& deg, // degree from G (O(1))
                          GlobalIncumbent& global) {
    const auto& Nv = H.sorted_neighbors(v);

    // Safe quick-stop: omega(v) <= deg(v)+1
    if (static_cast<int32_t>(Nv.size()) + 1 <= global.size()) return;

    // Thread-local buffers to avoid allocations in tight loops
    static thread_local std::vector<int32_t> tmp;
    static thread_local std::vector<int32_t> N;
    static thread_local std::vector<int32_t> C;

    tmp.clear();
    N.clear();
    C.clear();

    N.reserve(Nv.size());
    C.reserve(64);

    // Line 4: N <- { u in N_G(v) : |N_G(u)| >= |C*| }
    const int32_t incumbent_sz = global.size();
    for (int32_t u : Nv) {
        if (deg[static_cast<std::size_t>(u)] >= incumbent_sz) {
            N.push_back(u);
        }
    }

    // Line 5
    C.push_back(v);

    // Lines 6-11
    while (!N.empty()) {
        int32_t best_u = -1;
        int32_t theta = -1; // running maximum (paper's θ)

        // Line 8: u <- argmax_{w in N} |N ∩ N(w)| using intersect-gt with θ
        for (int32_t w : N) {
            const auto& Bw = H.hashed_neighbors(w);
            const int32_t d = intersect_gt(N, Bw, tmp, theta);
            if (d > theta) {
                theta = d;
                best_u = w;
            }
        }

        if (best_u < 0) break;

        // Line 9
        C.push_back(best_u);

        // Line 10: N <- N ∩ N(u)
        const auto& Bu = H.hashed_neighbors(best_u);
        const int32_t d = intersect_gt(N, Bu, tmp, 0);

        if (d <= 0) {
            N.clear();
        } else {
            N.assign(tmp.begin(), tmp.begin() + d);
            N.erase(std::remove(N.begin(), N.end(), best_u), N.end());
        }
    }

    // Lines 12-13
    global.try_update(std::move(C));
}

} // namespace

// Wrapper overload expected by the rest of your code/tests.
std::vector<int32_t> degree_based_heuristic(const AdjList& G, int top_k) {
    if (G.empty() || top_k <= 0) return {};

    const int32_t n = static_cast<int32_t>(G.size());
    const int32_t k = std::min<int32_t>(top_k, n);

    // Degrees from AdjList: O(n), cheap, no LazyGraph touching.
    std::vector<int32_t> deg(static_cast<std::size_t>(n), 0);
    for (int32_t v = 0; v < n; ++v) {
        deg[static_cast<std::size_t>(v)] =
            static_cast<int32_t>(G[static_cast<std::size_t>(v)].size());
    }

    // Identify top-K vertices with highest degree (Algorithm 5 line 2)
    std::vector<int32_t> verts(static_cast<std::size_t>(n));
    for (int32_t v = 0; v < n; ++v) verts[static_cast<std::size_t>(v)] = v;

    std::nth_element(
        verts.begin(),
        verts.begin() + k,
        verts.end(),
        [&](int32_t a, int32_t b) { return deg[static_cast<std::size_t>(a)] > deg[static_cast<std::size_t>(b)]; });

    verts.resize(static_cast<std::size_t>(k));

    // Optional but helpful: sort only the top-K for stronger pruning/early shrink
    std::sort(
        verts.begin(),
        verts.end(),
        [&](int32_t a, int32_t b) { return deg[static_cast<std::size_t>(a)] > deg[static_cast<std::size_t>(b)]; });

    // Identity relabeling: H-ids == G-ids.
    std::vector<int32_t> relabel_to_original(n);
    std::vector<int32_t> original_to_relabel(n);
    std::vector<int32_t> core(n, 0);
    for (int32_t i = 0; i < n; ++i) {
        relabel_to_original[i] = i;
        original_to_relabel[i] = i;
    }
    LazyGraph H(G, relabel_to_original, original_to_relabel, core);

    GlobalIncumbent global;

    // parfor v in top-K (Algorithm 5 line 3)
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

                const int32_t v = verts[static_cast<std::size_t>(i)];
                const int32_t deg_v = deg[static_cast<std::size_t>(v)];

                // Safe prune: if deg(v)+1 <= |C*|, skip this v
                if (deg_v + 1 <= global.size()) continue;

                run_candidate(H, v, deg, global);
            }
        });
    }

    for (auto& th : pool) th.join();

    std::shared_lock<std::shared_mutex> lock(global.mtx);
    return global.best;
}

// If you still need this overload elsewhere, keep it (uses degrees from original graph anyway).
std::vector<int32_t> degree_based_heuristic(LazyGraph& H, int top_k) {
    // Prefer calling the AdjList overload above in your pipeline.
    // Keeping this for API compatibility: fall back to building deg from H.original_graph().
    return degree_based_heuristic(H.original_graph(), top_k);
}

} // namespace lazymc
