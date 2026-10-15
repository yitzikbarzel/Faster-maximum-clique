#include <gtest/gtest.h>

#include "lazymc/lazymc.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <thread>
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

// ---------------------------------------------------------------------------
// Wider coverage: sparse to very dense graphs, larger n.
// ---------------------------------------------------------------------------

namespace {

void ref_expand(const std::vector<std::uint64_t>& adj, std::uint64_t P, int size, int& best) {
    if (P == 0) {
        best = std::max(best, size);
        return;
    }
    while (P) {
        if (size + __builtin_popcountll(P) <= best) return;
        const int v = __builtin_ctzll(P);
        P &= P - 1;
        ref_expand(adj, P & adj[static_cast<std::size_t>(v)], size + 1, best);
    }
}

// Exact maximum clique size for n <= 64.
int reference_max_clique(const AdjList& G) {
    std::vector<std::uint64_t> adj(G.size(), 0);
    for (std::size_t v = 0; v < G.size(); ++v) {
        for (int32_t u : G[v]) {
            if (static_cast<std::size_t>(u) != v) adj[v] |= std::uint64_t{1} << u;
        }
    }
    const std::uint64_t all = G.size() == 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << G.size()) - 1);
    int best = 0;
    ref_expand(adj, all, 0, best);
    return best;
}

} // namespace

TEST(LazyMC_Correctness, RandomGraphsAcrossDensities) {
    std::mt19937 rng(2024);
    for (double p : {0.05, 0.2, 0.4, 0.6, 0.75, 0.85, 0.92, 0.97}) {
        for (int trial = 0; trial < 12; ++trial) {
            const int n = 16 + 4 * trial; // 16..60
            AdjList G = make_random_graph(n, p, rng);

            const std::vector<int32_t> C = lazymc::LazyMC_run(G);
            ASSERT_TRUE(is_clique_in_G(G, C)) << "n=" << n << " p=" << p;
            EXPECT_EQ(static_cast<int>(C.size()), reference_max_clique(G))
                << "n=" << n << " p=" << p << " trial=" << trial;
        }
    }
}

// Dense communities inside a sparse graph, so that the dense (vertex cover)
// path of NeighborSearch is exercised as part of the full pipeline.
TEST(LazyMC_Correctness, DenseCommunitiesInSparseGraph) {
    std::mt19937 rng(99);
    for (int trial = 0; trial < 30; ++trial) {
        const int n = 64;
        AdjList G = make_random_graph(n, 0.05, rng);
        std::uniform_int_distribution<int> any(0, n - 1);
        std::bernoulli_distribution keep(0.8 + 0.01 * (trial % 15));
        for (int c = 0; c < 3; ++c) {
            std::vector<int> members;
            while (members.size() < 14) {
                const int v = any(rng);
                if (std::find(members.begin(), members.end(), v) == members.end()) members.push_back(v);
            }
            for (std::size_t i = 0; i < members.size(); ++i) {
                for (std::size_t j = i + 1; j < members.size(); ++j) {
                    if (keep(rng) && !has_edge(G, members[i], members[j])) {
                        G[static_cast<std::size_t>(members[i])].push_back(members[j]);
                        G[static_cast<std::size_t>(members[j])].push_back(members[i]);
                    }
                }
            }
        }

        const std::vector<int32_t> C = lazymc::LazyMC_run(G);
        ASSERT_TRUE(is_clique_in_G(G, C)) << "trial=" << trial;
        EXPECT_EQ(static_cast<int>(C.size()), reference_max_clique(G)) << "trial=" << trial;
    }
}

// Unsorted lists, duplicates and self-loops must not change the answer.
TEST(LazyMC_Correctness, UnnormalizedInputIsHandled) {
    std::mt19937 rng(5);
    for (int trial = 0; trial < 20; ++trial) {
        const int n = 40;
        const AdjList clean = make_random_graph(n, 0.5, rng);
        const int expected = reference_max_clique(clean);

        AdjList messy = clean;
        for (int v = 0; v < n; ++v) {
            auto& nbrs = messy[static_cast<std::size_t>(v)];
            if (!nbrs.empty() && v % 3 == 0) nbrs.push_back(nbrs.front()); // duplicate
            if (v % 5 == 0) nbrs.push_back(v);                                // self-loop
            std::shuffle(nbrs.begin(), nbrs.end(), rng);
        }

        const std::vector<int32_t> C = lazymc::LazyMC_run(messy);
        ASSERT_TRUE(is_clique_in_G(clean, C)) << "trial=" << trial;
        // No repeated vertices.
        std::vector<int32_t> sorted = C;
        std::sort(sorted.begin(), sorted.end());
        EXPECT_TRUE(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
        EXPECT_EQ(static_cast<int>(C.size()), expected) << "trial=" << trial;
    }
}

// Runs must not influence each other (there used to be a global incumbent).
TEST(LazyMC_Correctness, ConsecutiveAndConcurrentRunsAreIndependent) {
    std::mt19937 rng(77);
    const AdjList big = make_random_graph(60, 0.9, rng);
    const AdjList small = make_random_graph(20, 0.2, rng);
    const int big_omega = reference_max_clique(big);
    const int small_omega = reference_max_clique(small);

    EXPECT_EQ(static_cast<int>(lazymc::LazyMC_run(big).size()), big_omega);
    EXPECT_EQ(static_cast<int>(lazymc::LazyMC_run(small).size()), small_omega);

    std::vector<int32_t> a, b;
    std::thread ta([&]() { a = lazymc::LazyMC_run(big); });
    std::thread tb([&]() { b = lazymc::LazyMC_run(small); });
    ta.join();
    tb.join();
    EXPECT_EQ(static_cast<int>(a.size()), big_omega);
    EXPECT_EQ(static_cast<int>(b.size()), small_omega);
    EXPECT_TRUE(is_clique_in_G(small, b));
}

// End-to-end regression for the dense (vertex cover) path. The maximum clique
// has 6 vertices (30, 31 and one of p_i/q_i from each triangle below) and sits
// in the dense neighborhood of vertex 30:
// in the complement of N+(30), 31 is adjacent to 32..35 and each of those
// forms a triangle with two private vertices. Covering that complement by
// taking 31 first gives a non-minimum cover and only a 5-clique.
// Decoys keep the heuristics from finding the answer first:
//   - K_{75,75} on 100..249: highest degrees, triangle-free;
//   - K_{10,10} on 0..19: same coreness as the hidden structure but lower ids,
//     so the coreness heuristic starts there.
TEST(LazyMC_Correctness, HiddenCliqueBehindNonMinimumVertexCover) {
    const int n = 250;
    AdjList G(static_cast<std::size_t>(n));
    auto add = [&](int a, int b) {
        G[static_cast<std::size_t>(a)].push_back(b);
        G[static_cast<std::size_t>(b)].push_back(a);
    };
    for (int a = 0; a < 10; ++a)
        for (int b = 10; b < 20; ++b) add(a, b);

    const int o = 30; // v' = o, w = o+1, u_i = o+2+i, p_i = o+6+i, q_i = o+10+i
    std::vector<std::vector<bool>> comp(14, std::vector<bool>(14, false));
    for (int i = 0; i < 4; ++i) {
        const int u = 2 + i, p = 6 + i, q = 10 + i;
        for (auto [a, b] : {std::pair<int, int>{1, u}, {u, p}, {u, q}, {p, q}}) {
            comp[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)] = true;
            comp[static_cast<std::size_t>(b)][static_cast<std::size_t>(a)] = true;
        }
    }
    for (int x = 1; x < 14; ++x) add(o, o + x);
    for (int a = 1; a < 14; ++a)
        for (int b = a + 1; b < 14; ++b)
            if (!comp[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]) add(o + a, o + b);

    for (int a = 100; a < 175; ++a)
        for (int b = 175; b < 250; ++b) add(a, b);

    for (int run = 0; run < 3; ++run) {
        const std::vector<int32_t> C = lazymc::LazyMC_run(G);
        ASSERT_TRUE(is_clique_in_G(G, C));
        EXPECT_EQ(C.size(), 6u) << "run=" << run;
    }
}
