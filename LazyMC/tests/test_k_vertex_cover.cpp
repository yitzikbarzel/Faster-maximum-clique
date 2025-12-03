#include "lazymc/k_vertex_cover.h"
#include <gtest/gtest.h>

// Small helper to check if a set of vertices is a vertex cover of G.
static bool is_vertex_cover(const lazymc::AdjList& G,
                            const std::vector<int32_t>& cover) {
    std::vector<bool> in_cover(G.size(), false);
    for (int32_t v : cover) {
        if (v >= 0 && static_cast<size_t>(v) < in_cover.size()) {
            in_cover[v] = true;
        }
    }

    for (int32_t u = 0; u < static_cast<int32_t>(G.size()); ++u) {
        for (int32_t v : G[u]) {
            if (u < v) { // check each undirected edge once
                if (!in_cover[u] && !in_cover[v]) {
                    return false; // edge (u,v) is not covered
                }
            }
        }
    }
    return true;
}

TEST(KVertexCoverBasic, EmptyGraphKZero) {
    lazymc::AdjList G(1); // one vertex, no edges
    int32_t k = 0;

    auto res = lazymc::k_vertex_cover_adj(G, k);

    EXPECT_TRUE(res.found);
    EXPECT_TRUE(res.cover.empty());
    EXPECT_TRUE(is_vertex_cover(G, res.cover));
}

TEST(KVertexCoverBasic, SingleEdgeKOne) {
    lazymc::AdjList G(2);
    G[0].push_back(1);
    G[1].push_back(0);

    int32_t k = 1;
    auto res = lazymc::k_vertex_cover_adj(G, k);

    EXPECT_TRUE(res.found);
    EXPECT_LE(res.cover.size(), 1u);
    EXPECT_TRUE(is_vertex_cover(G, res.cover));
}

TEST(KVertexCoverBasic, SingleEdgeKZero) {
    lazymc::AdjList G(2);
    G[0].push_back(1);
    G[1].push_back(0);

    int32_t k = 0;
    auto res = lazymc::k_vertex_cover_adj(G, k);

    EXPECT_FALSE(res.found);
}

TEST(KVertexCoverBasic, TriangleKTwo) {
    // Triangle 0-1-2 complete graph, minimum vertex cover size is 2.
    lazymc::AdjList G(3);
    G[0] = {1, 2};
    G[1] = {0, 2};
    G[2] = {0, 1};

    int32_t k = 2;
    auto res = lazymc::k_vertex_cover_adj(G, k);

    EXPECT_TRUE(res.found);
    EXPECT_LE(res.cover.size(), 2u);
    EXPECT_TRUE(is_vertex_cover(G, res.cover));
}

TEST(KVertexCoverBasic, PathGraph) {
    // Path 0-1-2-3
    lazymc::AdjList G(4);
    G[0] = {1};
    G[1] = {0, 2};
    G[2] = {1, 3};
    G[3] = {2};

    // Minimum vertex cover size is 2 (e.g. {1,2})

    // With k = 1 => should fail
    {
        int32_t k = 1;
        auto res = lazymc::k_vertex_cover_adj(G, k);
        EXPECT_FALSE(res.found);
    }

    // With k = 2 => should succeed
    {
        int32_t k = 2;
        auto res = lazymc::k_vertex_cover_adj(G, k);
        EXPECT_TRUE(res.found);
        EXPECT_LE(res.cover.size(), 2u);
        EXPECT_TRUE(is_vertex_cover(G, res.cover));
    }
}
