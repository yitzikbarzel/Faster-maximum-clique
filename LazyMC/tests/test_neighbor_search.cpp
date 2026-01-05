#include <gtest/gtest.h>

#include "lazymc/neighbor_search.h"
#include "lazymc/lazygraph.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// Type alias consistent with your code
using AdjList = std::vector<std::vector<int32_t>>;

namespace {

// Helper: build an undirected edge between u and v.
void add_undirected_edge(AdjList& G, int32_t u, int32_t v) {
    G[static_cast<std::size_t>(u)].push_back(v);
    G[static_cast<std::size_t>(v)].push_back(u);
}

static lazymc::LazyGraph MakeIdentityLazyGraph(const AdjList& G,
                                               const std::vector<int32_t>& core_H) {
    const int32_t n = static_cast<int32_t>(G.size());
    std::vector<int32_t> r2o(n), o2r(n);
    for (int32_t i = 0; i < n; ++i) {
        r2o[i] = i;
        o2r[i] = i;
    }
    return lazymc::LazyGraph(G, r2o, o2r, core_H);
}

static std::vector<int32_t> DummyCliqueOfSize(int32_t sz) {
    std::vector<int32_t> c;
    c.reserve(static_cast<std::size_t>(sz));
    for (int32_t i = 0; i < sz; ++i) c.push_back(i);
    return c;
}

} // namespace

using lazymc::NeighborSearch;

// ---------------------------------------------------------------------------
// Filter 1 tests
// ---------------------------------------------------------------------------

TEST(NeighborSearchFilter1, NoQualifiedNeighborsReturnsEarly) {
    AdjList G(2);
    add_undirected_edge(G, 0, 1);

    std::vector<int32_t> core_H = {1, 1};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(2);
    double phi = 0.5;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 2u);
}

TEST(NeighborSearchFilter1, NTooSmallPrunesSearch) {
    AdjList G(3);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 1, 2);

    std::vector<int32_t> core_H = {3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(3);
    double phi = 0.5;

    NeighborSearch(H, /*vH=*/1, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 3u);
}

// ---------------------------------------------------------------------------
// Filter 2 tests
// ---------------------------------------------------------------------------

TEST(NeighborSearchFilter2, RemovesVerticesWithSmallIntersection) {
    AdjList G(4);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);

    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);

    std::vector<int32_t> core_H = {3, 3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(3);
    double phi = 0.5;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 3u);
}

TEST(NeighborSearchFilter2, KeepsVerticesWithLargeIntersection) {
    AdjList G(4);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);
    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);
    add_undirected_edge(G, 2, 3);

    std::vector<int32_t> core_H = {3, 3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(2);
    double phi = 0.9;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 4u);
}

// ---------------------------------------------------------------------------
// Filter 3 + density tests
// ---------------------------------------------------------------------------

TEST(NeighborSearchFilter3, ShrinksNButKeepsEnoughVerticesAndFindsClique) {
    AdjList G(6);

    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);
    add_undirected_edge(G, 0, 4);
    add_undirected_edge(G, 0, 5);

    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);
    add_undirected_edge(G, 2, 3);

    add_undirected_edge(G, 1, 4);
    add_undirected_edge(G, 2, 5);

    std::vector<int32_t> core_H(6, 10);
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(3);
    double phi = 0.5;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 4u);
}

TEST(NeighborSearchDensity, DenseCaseUsesKVertexCoverBranchAndFindsClique) {
    AdjList G(4);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);
    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);
    add_undirected_edge(G, 2, 3);

    std::vector<int32_t> core_H = {3, 3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(2);
    double phi = 0.1;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 4u);
}

TEST(NeighborSearchDensity, SparseCaseUsesMaximumCliqueFallback) {
    AdjList G(4);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 0, 2);
    add_undirected_edge(G, 0, 3);
    add_undirected_edge(G, 1, 2);
    add_undirected_edge(G, 1, 3);
    add_undirected_edge(G, 2, 3);

    std::vector<int32_t> core_H = {3, 3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> best_clique_G = DummyCliqueOfSize(2);
    double phi = 0.9;

    NeighborSearch(H, /*vH=*/0, core_H, best_clique_G, phi);

    EXPECT_EQ(best_clique_G.size(), 4u);
}
