#include <gtest/gtest.h>

#include "lazymc/systematic_search.h"
#include "lazymc/neighbor_search.h"
#include "lazymc/lazygraph.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using AdjList = std::vector<std::vector<int32_t>>;

namespace {

void add_undirected_edge(AdjList& G, int32_t u, int32_t v) {
    G[static_cast<std::size_t>(u)].push_back(v);
    G[static_cast<std::size_t>(v)].push_back(u);
}

bool is_edge(const AdjList& G, int32_t u, int32_t v) {
    const auto& nbrs = G[static_cast<std::size_t>(u)];
    return std::find(nbrs.begin(), nbrs.end(), v) != nbrs.end();
}

bool is_clique_in_G(const AdjList& G, const std::vector<int32_t>& C) {
    for (std::size_t i = 0; i < C.size(); ++i) {
        int32_t a = C[i];
        if (a < 0 || static_cast<std::size_t>(a) >= G.size()) return false;
        for (std::size_t j = i + 1; j < C.size(); ++j) {
            int32_t b = C[j];
            if (b < 0 || static_cast<std::size_t>(b) >= G.size()) return false;
            if (!is_edge(G, a, b) || !is_edge(G, b, a)) return false;
        }
    }
    return true;
}

lazymc::LazyGraph make_identity_H(const AdjList& G,
                                 const std::vector<int32_t>& core_H) {
    const int32_t n = static_cast<int32_t>(G.size());
    std::vector<int32_t> r2o(n), o2r(n);
    for (int32_t i = 0; i < n; ++i) {
        r2o[i] = i;
        o2r[i] = i;
    }
    return lazymc::LazyGraph(G, r2o, o2r, core_H);
}

} // namespace

// -------------------------
// Test 1: empty graph
// -------------------------
TEST(SystematicSearch, EmptyGraphReturnsEmpty) {
    AdjList G;
    std::vector<int32_t> core_H;
    auto H = make_identity_H(G, core_H);

    auto C = lazymc::systematic_search(H, core_H);
    EXPECT_TRUE(C.empty());
}

// -------------------------
// Test 2: K4 should return 4
// -------------------------
TEST(SystematicSearch, K4FindsCliqueOfSize4) {
    AdjList G(4);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);
    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);
    add_undirected_edge(G, 2, 3);

    // give everyone high core so filters won't block
    std::vector<int32_t> core_H = {10, 10, 10, 10};
    auto H = make_identity_H(G, core_H);

    auto C = lazymc::systematic_search(H, core_H);

    EXPECT_TRUE(is_clique_in_G(G, C));
    EXPECT_EQ(C.size(), 4u);
}

// -------------------------
// Test 3: two components - pick bigger clique
// -------------------------
TEST(SystematicSearch, DisconnectedComponentsPicksLargestClique) {
    // component A: triangle (0,1,2)
    // component B: edge (3,4)
    AdjList G(5);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 1, 2);

    add_undirected_edge(G, 3, 4);

    std::vector<int32_t> core_H = {10, 10, 10, 10, 10};
    auto H = make_identity_H(G, core_H);

    auto C = lazymc::systematic_search(H, core_H);

    EXPECT_TRUE(is_clique_in_G(G, C));
    EXPECT_EQ(C.size(), 3u);
}

// -------------------------
// Test 4: no edges => at most 1
// -------------------------
TEST(SystematicSearch, EmptyEdgeGraphReturnsAtMostOneVertex) {
    AdjList G(6);
    std::vector<int32_t> core_H(6, 10);
    auto H = make_identity_H(G, core_H);

    auto C = lazymc::systematic_search(H, core_H);

    EXPECT_TRUE(is_clique_in_G(G, C));
    EXPECT_LE(C.size(), 1u);
}
