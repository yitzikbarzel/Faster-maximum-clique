#include "lazymc/degree_heuristic.h"
#include "lazymc/incumbent.h"
#include "lazymc/lazygraph.h"
#include "lazymc/normalize.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace lazymc {

namespace {

// Per-thread state for Algorithm 5.
//
// The argmax in line 8 needs |N ∩ N(w)| for every w in N. Recomputing those
// counts at every step costs O(|N|^2) hash lookups per step on hub vertices.
// Instead the counts are computed once and then maintained: when a vertex
// leaves N, the counts of its neighbors still in N drop by one. A whole run
// from v then costs about twice the summed degree of N(v). Membership in N
// is a bitset over all vertices (n/8 bytes, small enough to stay in cache).
class DegreeWorker {
public:
    DegreeWorker(const AdjList& G, const std::vector<int32_t>& deg, Incumbent& incumbent)
        : G_(G),
          deg_(deg),
          incumbent_(incumbent),
          in_N_((G.size() + 63) / 64, 0),
          in_next_((G.size() + 63) / 64, 0),
          pos_(G.size(), 0) {}

    // Greedy clique growth from v (Algorithm 5, lines 4-13).
    void run(int32_t v) {
        const auto& Nv = G_[static_cast<std::size_t>(v)];

        // Safe quick-stop: omega(v) <= deg(v) + 1
        if (static_cast<int32_t>(Nv.size()) + 1 <= incumbent_.size()) return;

        // Line 4: N <- { u in N_G(v) : |N_G(u)| >= |C*| }
        const int32_t incumbent_sz = incumbent_.size();
        N_.clear();
        for (int32_t u : Nv) {
            if (deg_[static_cast<std::size_t>(u)] >= incumbent_sz) N_.push_back(u);
        }

        // Line 5
        C_.clear();
        C_.push_back(v);

        // count_[pos_[w]] = |N ∩ N(w)| for w in N.
        for (std::size_t i = 0; i < N_.size(); ++i) {
            set(in_N_, N_[i]);
            pos_[static_cast<std::size_t>(N_[i])] = static_cast<int32_t>(i);
        }
        count_.assign(N_.size(), 0);
        for (std::size_t i = 0; i < N_.size(); ++i) {
            int32_t c = 0;
            for (int32_t x : G_[static_cast<std::size_t>(N_[i])]) c += test(in_N_, x);
            count_[i] = c;
        }

        // Lines 6-11
        while (!N_.empty()) {
            // C plus all of N cannot beat the incumbent: stop early.
            if (static_cast<int32_t>(C_.size() + N_.size()) <= incumbent_.size()) break;

            // Line 8: u <- argmax_{w in N} |N ∩ N(w)|.
            // Adding w can at most lead to a clique of |C| + 1 + |N ∩ N(w)|,
            // so candidates with |N ∩ N(w)| <= |C*| - |C| - 2 cannot matter.
            // If no candidate is left, this run cannot beat C*.
            int32_t best_u = -1;
            int32_t theta = std::max(-1, incumbent_.size() - static_cast<int32_t>(C_.size()) - 2);
            for (int32_t w : N_) {
                const int32_t d = count_[static_cast<std::size_t>(pos_[static_cast<std::size_t>(w)])];
                if (d > theta) {
                    theta = d;
                    best_u = w;
                }
            }
            if (best_u < 0) break;

            // Line 9
            C_.push_back(best_u);

            // Line 10: N <- N ∩ N(u), keeping N's order
            for (int32_t x : G_[static_cast<std::size_t>(best_u)]) {
                if (test(in_N_, x)) set(in_next_, x);
            }
            next_.clear();
            removed_.clear();
            for (int32_t x : N_) {
                if (test(in_next_, x)) {
                    next_.push_back(x);
                    clear(in_next_, x);
                } else {
                    removed_.push_back(x);
                    clear(in_N_, x);
                }
            }
            for (int32_t r : removed_) {
                for (int32_t y : G_[static_cast<std::size_t>(r)]) {
                    if (test(in_N_, y)) --count_[static_cast<std::size_t>(pos_[static_cast<std::size_t>(y)])];
                }
            }
            N_.swap(next_);
        }
        for (int32_t x : N_) clear(in_N_, x);

        // Lines 12-13
        if (static_cast<int32_t>(C_.size()) > incumbent_.size()) incumbent_.try_update(C_);
    }

private:
    using Bits = std::vector<std::uint64_t>;
    static void set(Bits& b, int32_t u) { b[static_cast<std::size_t>(u) >> 6] |= std::uint64_t{1} << (u & 63); }
    static void clear(Bits& b, int32_t u) { b[static_cast<std::size_t>(u) >> 6] &= ~(std::uint64_t{1} << (u & 63)); }
    static int32_t test(const Bits& b, int32_t u) {
        return static_cast<int32_t>((b[static_cast<std::size_t>(u) >> 6] >> (u & 63)) & 1u);
    }

    const AdjList& G_;
    const std::vector<int32_t>& deg_;
    Incumbent& incumbent_;

    Bits in_N_;                   // members of N
    Bits in_next_;                // scratch for N ∩ N(u)
    std::vector<int32_t> pos_;    // index of a member of N into count_
    std::vector<int32_t> count_;  // |N ∩ N(w)|, indexed by position in the initial N
    std::vector<int32_t> N_;
    std::vector<int32_t> next_;
    std::vector<int32_t> removed_;
    std::vector<int32_t> C_;
};

} // namespace

std::vector<int32_t> degree_based_heuristic(const AdjList& input, int top_k) {
    if (input.empty() || top_k <= 0) return {};

    // The intersections below need sorted, duplicate-free lists.
    AdjList normalized;
    const AdjList* graph = &input;
    if (!is_normalized(input)) {
        normalized = normalized_copy(input);
        graph = &normalized;
    }
    const AdjList& G = *graph;

    const int32_t n = static_cast<int32_t>(G.size());
    const int32_t k = std::min<int32_t>(top_k, n);

    std::vector<int32_t> deg(static_cast<std::size_t>(n), 0);
    for (int32_t v = 0; v < n; ++v) {
        deg[static_cast<std::size_t>(v)] =
            static_cast<int32_t>(G[static_cast<std::size_t>(v)].size());
    }

    // Identify top-K vertices with highest degree (Algorithm 5 line 2)
    std::vector<int32_t> verts(static_cast<std::size_t>(n));
    for (int32_t v = 0; v < n; ++v) verts[static_cast<std::size_t>(v)] = v;

    auto by_degree_desc = [&](int32_t a, int32_t b) {
        return deg[static_cast<std::size_t>(a)] > deg[static_cast<std::size_t>(b)];
    };
    if (k < n) {
        std::nth_element(verts.begin(), verts.begin() + k, verts.end(), by_degree_desc);
        verts.resize(static_cast<std::size_t>(k));
    }
    // Highest degree first: finds large cliques early, which prunes the rest.
    std::sort(verts.begin(), verts.end(), by_degree_desc);

    Incumbent incumbent;

    // parfor v in top-K (Algorithm 5 line 3)
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workers = std::min<unsigned>(hw, static_cast<unsigned>(k));

    std::atomic<int32_t> next{0};
    std::vector<std::thread> pool;
    pool.reserve(workers);

    for (unsigned t = 0; t < workers; ++t) {
        pool.emplace_back([&]() {
            DegreeWorker worker(G, deg, incumbent);
            while (true) {
                const int32_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= k) break;

                const int32_t v = verts[static_cast<std::size_t>(i)];
                // Safe prune: if deg(v)+1 <= |C*|, skip this v
                if (deg[static_cast<std::size_t>(v)] + 1 <= incumbent.size()) continue;

                worker.run(v);
            }
        });
    }

    for (auto& th : pool) th.join();

    return incumbent.clique();
}

std::vector<int32_t> degree_based_heuristic(LazyGraph& H, int top_k) {
    return degree_based_heuristic(H.original_graph(), top_k);
}

} // namespace lazymc
