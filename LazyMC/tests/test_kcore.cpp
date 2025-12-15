#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "lazymc/kcore.h"
#include "lazymc/lazygraph.h" // AdjList

using lazymc::AdjList;
using lazymc::KCore;

namespace {

// Build an undirected graph from edges (u,v). Assumes 0 <= u,v < n.
AdjList MakeUndirectedGraph(std::size_t n, const std::vector<std::pair<int32_t, int32_t>>& edges) {
    AdjList G(n);
    for (auto [u, v] : edges) {
        G[static_cast<std::size_t>(u)].push_back(v);
        G[static_cast<std::size_t>(v)].push_back(u);
    }
    return G;
}

// For each k, the induced subgraph of vertices with core >= k must have min-degree >= k.
// This is the defining property of core numbers (for the computed coreness values).
void ExpectCorePropertyHolds(const AdjList& G, const std::vector<int32_t>& core) {
    const std::size_t n = G.size();
    ASSERT_EQ(core.size(), n);

    int32_t max_core = 0;
    for (int32_t c : core) max_core = std::max(max_core, c);

    for (int32_t k = 1; k <= max_core; ++k) {
        std::vector<std::uint8_t> inS(n, static_cast<std::uint8_t>(0));
        std::size_t countS = 0;
        for (std::size_t v = 0; v < n; ++v) {
            if (core[v] >= k) {
                inS[v] = static_cast<std::uint8_t>(1);
                ++countS;
            }
        }

        // Empty set is fine.
        if (countS == 0) continue;

        // Check min degree within the induced subgraph.
        for (std::size_t v = 0; v < n; ++v) {
            if (inS[v] == 0) continue;

            int32_t deg_in_S = 0;
            for (int32_t u_i32 : G[v]) {
                const std::size_t u = static_cast<std::size_t>(u_i32);
                if (u < n && inS[u] != 0) {
                    ++deg_in_S;
                }
            }
            EXPECT_GE(deg_in_S, k) << "Vertex " << v << " violates k-core property for k=" << k;
        }
    }
}

} // namespace

// ------------------------------------------------------------
// Exact-value tests on small graphs
// ------------------------------------------------------------

TEST(KCoreExact, PathOf3_AllCore1) {
    // 0 - 1 - 2
    AdjList G = MakeUndirectedGraph(3, {{0,1}, {1,2}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 3u);
    EXPECT_EQ(core[0], 1);
    EXPECT_EQ(core[1], 1);
    EXPECT_EQ(core[2], 1);

    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreExact, Triangle_AllCore2) {
    // 0 - 1 - 2 - 0
    AdjList G = MakeUndirectedGraph(3, {{0,1}, {1,2}, {2,0}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 3u);
    EXPECT_EQ(core[0], 2);
    EXPECT_EQ(core[1], 2);
    EXPECT_EQ(core[2], 2);

    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreExact, TriangleWithTail_TriangleCore2_TailCore1) {
    // Triangle 0-1-2-0 plus tail 3-0
    AdjList G = MakeUndirectedGraph(4, {{0,1}, {1,2}, {2,0}, {3,0}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 4u);
    EXPECT_EQ(core[0], 2);
    EXPECT_EQ(core[1], 2);
    EXPECT_EQ(core[2], 2);
    EXPECT_EQ(core[3], 1);

    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreExact, CycleOf4_AllCore2) {
    // 0-1-2-3-0
    AdjList G = MakeUndirectedGraph(4, {{0,1}, {1,2}, {2,3}, {3,0}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 4u);
    for (std::size_t v = 0; v < 4; ++v) {
        EXPECT_EQ(core[v], 2);
    }

    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreExact, CompleteGraphK4_AllCore3) {
    // K4: every vertex degree 3 => coreness 3
    AdjList G = MakeUndirectedGraph(4, {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 4u);
    for (std::size_t v = 0; v < 4; ++v) {
        EXPECT_EQ(core[v], 3);
    }

    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreExact, Star_AllCore1) {
    // Star with center 0 connected to 1..5
    AdjList G = MakeUndirectedGraph(6, {{0,1},{0,2},{0,3},{0,4},{0,5}});
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 6u);
    for (std::size_t v = 0; v < 6; ++v) {
        EXPECT_EQ(core[v], 1);
    }

    ExpectCorePropertyHolds(G, core);
}

// ------------------------------------------------------------
// min_degree post-filter behavior (matches paper condition)
// ------------------------------------------------------------

TEST(KCoreMinDegree, TriangleWithTail_MinDegree2_TailBecomes0) {
    // Triangle 0-1-2-0 plus tail 3-0
    // init_deg[3] = 1 < 2 => core[3] must be 0 after post-filter.
    AdjList G = MakeUndirectedGraph(4, {{0,1}, {1,2}, {2,0}, {3,0}});
    auto core = KCore(G, /*min_degree=*/2);

    ASSERT_EQ(core.size(), 4u);
    EXPECT_EQ(core[0], 2);
    EXPECT_EQ(core[1], 2);
    EXPECT_EQ(core[2], 2);
    EXPECT_EQ(core[3], 0);

    // Property should still hold for remaining vertices.
    ExpectCorePropertyHolds(G, core);
}

TEST(KCoreMinDegree, Star_MinDegree3_Leaves0_CenterStays1) {
    // Center has init degree 5 >= 3, leaves have init degree 1 < 3.
    AdjList G = MakeUndirectedGraph(6, {{0,1},{0,2},{0,3},{0,4},{0,5}});
    auto core = KCore(G, /*min_degree=*/3);

    ASSERT_EQ(core.size(), 6u);
    EXPECT_EQ(core[0], 1);
    for (std::size_t v = 1; v < 6; ++v) {
        EXPECT_EQ(core[v], 0);
    }

    
}

// ------------------------------------------------------------
// Basic robustness checks
// ------------------------------------------------------------

TEST(KCoreRobustness, EmptyGraph) {
    AdjList G;
    auto core = KCore(G, /*min_degree=*/0);
    EXPECT_TRUE(core.empty());
}

TEST(KCoreRobustness, SingleIsolatedVertex_Core0) {
    AdjList G(1);
    auto core = KCore(G, /*min_degree=*/0);

    ASSERT_EQ(core.size(), 1u);
    EXPECT_EQ(core[0], 0);

    ExpectCorePropertyHolds(G, core);
}
