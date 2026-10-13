#include <gtest/gtest.h>

#include "lazymc/clique_solver.h"
#include "lazymc/k_vertex_cover.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <random>
#include <vector>

using lazymc::AdjList;
using lazymc::BitGraph;

namespace {

// Reference: plain recursive search over 64-bit masks (n <= 64).
void reference_expand(const std::vector<std::uint64_t>& adj, std::uint64_t P, int size, int& best) {
    if (P == 0) {
        best = std::max(best, size);
        return;
    }
    if (size + __builtin_popcountll(P) <= best) return;
    while (P) {
        if (size + __builtin_popcountll(P) <= best) return;
        const int v = __builtin_ctzll(P);
        P &= P - 1;
        reference_expand(adj, P & adj[static_cast<std::size_t>(v)], size + 1, best);
    }
}

int reference_max_clique(const AdjList& G) {
    std::vector<std::uint64_t> adj(G.size(), 0);
    for (std::size_t v = 0; v < G.size(); ++v) {
        for (int32_t u : G[v]) adj[v] |= std::uint64_t{1} << u;
    }
    const std::uint64_t all = G.size() == 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << G.size()) - 1);
    int best = 0;
    reference_expand(adj, all, 0, best);
    return best;
}

AdjList random_graph(int n, double p, std::mt19937& rng) {
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

BitGraph to_bitgraph(const AdjList& G) {
    BitGraph bg(static_cast<int32_t>(G.size()));
    for (std::size_t v = 0; v < G.size(); ++v) {
        for (int32_t u : G[v]) bg.add_edge(static_cast<int32_t>(v), u);
    }
    return bg;
}

bool is_clique(const AdjList& G, const std::vector<int32_t>& C) {
    for (std::size_t i = 0; i < C.size(); ++i) {
        for (std::size_t j = i + 1; j < C.size(); ++j) {
            const auto& nbrs = G[static_cast<std::size_t>(C[i])];
            if (std::find(nbrs.begin(), nbrs.end(), C[j]) == nbrs.end()) return false;
        }
    }
    return true;
}

bool is_vertex_cover(const AdjList& G, const std::vector<int32_t>& cover) {
    std::vector<bool> in(G.size(), false);
    for (int32_t v : cover) in[static_cast<std::size_t>(v)] = true;
    for (std::size_t v = 0; v < G.size(); ++v) {
        for (int32_t u : G[v]) {
            if (!in[v] && !in[static_cast<std::size_t>(u)]) return false;
        }
    }
    return true;
}

AdjList complement(const AdjList& G) {
    const std::size_t n = G.size();
    std::vector<std::vector<bool>> a(n, std::vector<bool>(n, false));
    for (std::size_t v = 0; v < n; ++v) {
        for (int32_t u : G[v]) a[v][static_cast<std::size_t>(u)] = true;
    }
    AdjList C(n);
    for (std::size_t v = 0; v < n; ++v) {
        for (std::size_t u = 0; u < n; ++u) {
            if (u != v && !a[v][u]) C[v].push_back(static_cast<int32_t>(u));
        }
    }
    return C;
}

} // namespace

TEST(BitsetCliqueSolver, MatchesReferenceOnRandomGraphs) {
    std::mt19937 rng(7);
    for (double p : {0.1, 0.3, 0.5, 0.7, 0.9, 0.97}) {
        for (int trial = 0; trial < 15; ++trial) {
            const int n = 10 + trial * 3; // up to 52 vertices
            const AdjList G = random_graph(n, p, rng);
            const int expected = reference_max_clique(G);

            const auto C = lazymc::max_clique_bitset(to_bitgraph(G), 0);
            EXPECT_TRUE(is_clique(G, C));
            EXPECT_EQ(static_cast<int>(C.size()), expected) << "n=" << n << " p=" << p;
        }
    }
}

TEST(BitsetCliqueSolver, LowerBoundIsRespected) {
    std::mt19937 rng(11);
    const AdjList G = random_graph(40, 0.5, rng);
    const int omega = reference_max_clique(G);
    const BitGraph bg = to_bitgraph(G);

    EXPECT_EQ(static_cast<int>(lazymc::max_clique_bitset(bg, omega - 1).size()), omega);
    EXPECT_TRUE(lazymc::max_clique_bitset(bg, omega).empty());

    // A shared bound counts too, shifted by the offset.
    std::atomic<int32_t> shared{omega + 1};
    EXPECT_TRUE(lazymc::max_clique_bitset(bg, 0, &shared, 1).empty());
    EXPECT_EQ(static_cast<int>(lazymc::max_clique_bitset(bg, 0, &shared, 2).size()), omega);
}

TEST(BitsetCliqueSolver, EmptyAndEdgelessGraphs) {
    EXPECT_TRUE(lazymc::max_clique_bitset(BitGraph(0), 0).empty());
    EXPECT_EQ(lazymc::max_clique_bitset(BitGraph(5), 0).size(), 1u);
    EXPECT_TRUE(lazymc::max_clique_bitset(BitGraph(5), 1).empty());
}

TEST(AdjCliqueSolver, DecomposesLargeSparseGraphs) {
    // Larger than the direct bitset limit, so the degeneracy split is used.
    std::mt19937 rng(3);
    const int n = 12000;
    AdjList G(static_cast<std::size_t>(n));
    std::uniform_int_distribution<int> any(0, n - 1);
    for (int e = 0; e < 4 * n; ++e) {
        const int a = any(rng), b = any(rng);
        if (a == b) continue;
        G[static_cast<std::size_t>(a)].push_back(b);
        G[static_cast<std::size_t>(b)].push_back(a);
    }
    std::vector<int> planted;
    while (planted.size() < 9) {
        const int v = any(rng);
        if (std::find(planted.begin(), planted.end(), v) == planted.end()) planted.push_back(v);
    }
    for (std::size_t i = 0; i < planted.size(); ++i) {
        for (std::size_t j = i + 1; j < planted.size(); ++j) {
            G[static_cast<std::size_t>(planted[i])].push_back(planted[j]);
            G[static_cast<std::size_t>(planted[j])].push_back(planted[i]);
        }
    }
    for (auto& nbrs : G) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }

    const auto C = lazymc::max_clique_adj(G, 0);
    EXPECT_TRUE(is_clique(G, C));
    EXPECT_EQ(C.size(), 9u);
    EXPECT_TRUE(lazymc::max_clique_adj(G, 9).empty());
}

TEST(MinVertexCover, ComplementOfCoverIsMaximumClique) {
    std::mt19937 rng(5);
    for (double p : {0.6, 0.8, 0.9, 0.95}) {
        for (int trial = 0; trial < 10; ++trial) {
            const int n = 12 + trial * 3;
            const AdjList G = random_graph(n, p, rng);
            const AdjList comp = complement(G);
            const int omega = reference_max_clique(G);

            const auto res = lazymc::min_vertex_cover_adj(comp, n);
            ASSERT_TRUE(res.found);
            EXPECT_TRUE(res.optimal);
            EXPECT_TRUE(is_vertex_cover(comp, res.cover));
            EXPECT_EQ(static_cast<int>(res.cover.size()), n - omega) << "n=" << n << " p=" << p;

            // Nothing smaller than the minimum.
            const auto none = lazymc::min_vertex_cover_adj(comp, n - omega - 1);
            EXPECT_FALSE(none.found);
            EXPECT_FALSE(none.aborted);
        }
    }
}

TEST(MinVertexCover, FindsMinimumWhenBudgetIsLoose) {
    // Max-degree vertex 0 is in no minimum cover: 0 is adjacent to 1..4, and
    // each of 1..4 sits in a triangle with two private vertices. Minimum cover
    // = two vertices per triangle (8); taking 0 first costs 9.
    AdjList G(13);
    auto add = [&](int a, int b) {
        G[static_cast<std::size_t>(a)].push_back(b);
        G[static_cast<std::size_t>(b)].push_back(a);
    };
    for (int i = 0; i < 4; ++i) {
        const int u = 1 + i, p = 5 + i, q = 9 + i;
        add(0, u);
        add(u, p);
        add(u, q);
        add(p, q);
    }

    const auto res = lazymc::min_vertex_cover_adj(G, 12);
    ASSERT_TRUE(res.found);
    EXPECT_TRUE(res.optimal);
    EXPECT_TRUE(is_vertex_cover(G, res.cover));
    EXPECT_EQ(res.cover.size(), 8u);

    // The decision version still answers "is there a cover of size <= k".
    EXPECT_TRUE(lazymc::k_vertex_cover_adj(G, 12).found);
    EXPECT_TRUE(lazymc::k_vertex_cover_adj(G, 8).found);
    EXPECT_FALSE(lazymc::k_vertex_cover_adj(G, 7).found);
}

TEST(MinVertexCover, StepLimitReportsAbort) {
    std::mt19937 rng(9);
    const AdjList G = random_graph(60, 0.3, rng);
    const auto res = lazymc::min_vertex_cover_adj(G, 60, /*step_limit=*/1);
    EXPECT_TRUE(res.aborted);
    EXPECT_FALSE(res.optimal);
}
