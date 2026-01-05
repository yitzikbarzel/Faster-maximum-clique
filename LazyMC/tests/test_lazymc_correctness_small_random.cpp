#include <gtest/gtest.h>

#include "lazymc/lazymc.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace {

using AdjList = std::vector<std::vector<int32_t>>;

// Build an undirected Erdos–Renyi random graph G(n, p) with a fixed RNG.
static AdjList make_random_graph(int n, double p, std::mt19937& rng) {
    AdjList G(static_cast<std::size_t>(n));
    std::bernoulli_distribution edge(p);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (edge(rng)) {
                G[static_cast<std::size_t>(i)].push_back(j);
                G[static_cast<std::size_t>(j)].push_back(i);
            }
        }
    }
    return G;
}

static bool has_edge(const AdjList& G, int32_t u, int32_t v) {
    const auto& nbrs = G[static_cast<std::size_t>(u)];
    return std::find(nbrs.begin(), nbrs.end(), v) != nbrs.end();
}

static bool is_clique_in_G(const AdjList& G, const std::vector<int32_t>& C) {
    for (std::size_t i = 0; i < C.size(); ++i) {
        const int32_t a = C[i];
        if (a < 0 || static_cast<std::size_t>(a) >= G.size()) return false;

        for (std::size_t j = i + 1; j < C.size(); ++j) {
            const int32_t b = C[j];
            if (b < 0 || static_cast<std::size_t>(b) >= G.size()) return false;

            if (!has_edge(G, a, b) || !has_edge(G, b, a)) return false;
        }
    }
    return true;
}

// Exact maximum clique size by brute force using bitmasks.
// Intended for n <= 20 (we will use n=12 in tests).
static int exact_max_clique_size(const AdjList& G) {
    const int n = static_cast<int>(G.size());
    if (n <= 0) return 0;
    if (n > 20) {
        // This test is designed for small n only.
        // If you increase n, replace this with a faster exact algorithm.
        return -1;
    }

    std::vector<std::uint32_t> adj(static_cast<std::size_t>(n), 0u);
    for (int v = 0; v < n; ++v) {
        std::uint32_t mask = 0u;
        for (int32_t u : G[static_cast<std::size_t>(v)]) {
            if (u >= 0 && u < n) mask |= (1u << static_cast<std::uint32_t>(u));
        }
        adj[static_cast<std::size_t>(v)] = mask;
    }

    const std::uint32_t total = 1u << static_cast<std::uint32_t>(n);
    int best = 0;

    // Enumerate all subsets; check clique incrementally.
    // Complexity: O(2^n * n), fine for n=12.
    for (std::uint32_t S = 1u; S < total; ++S) {
        const int sz = __builtin_popcount(S);
        if (sz <= best) continue;

        // Clique check: for each vertex v in S, all other vertices in S must be in adj[v]
        bool ok = true;
        std::uint32_t T = S;
        while (T) {
            const int v = __builtin_ctz(T);
            T &= (T - 1u);

            const std::uint32_t others = S & ~(1u << static_cast<std::uint32_t>(v));
            if ((adj[static_cast<std::size_t>(v)] & others) != others) {
                ok = false;
                break;
            }
        }

        if (ok) best = sz;
    }

    return best;
}

} // namespace

TEST(LazyMC_Correctness, SmallRandomGraphsMatchExactMaximumClique) {
    constexpr std::uint32_t seed = 1337;
    std::mt19937 rng(seed);

    const int trials = 40;
    const int n = 12;
    const double p = 0.35;

    for (int t = 0; t < trials; ++t) {
        AdjList G = make_random_graph(n, p, rng);

        // Run your implementation
        std::vector<int32_t> C = lazymc::LazyMC_run(G);

        // 1) Always must return a valid clique
        ASSERT_TRUE(is_clique_in_G(G, C))
            << "Returned set is not a clique on trial " << t << " (seed=" << seed << ")";

        // 2) For small n, it should match the exact maximum clique size
        const int opt = exact_max_clique_size(G);
        ASSERT_GE(opt, 0) << "Exact solver refused n=" << n;

        EXPECT_EQ(static_cast<int>(C.size()), opt)
            << "Mismatch on trial " << t << " (seed=" << seed << ")";
    }
}
