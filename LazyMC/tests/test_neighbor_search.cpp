#include <gtest/gtest.h>

#include "lazymc/neighbor_search.h"
#include "lazymc/lazygraph.h"

#include <algorithm>
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

bool IsCliqueInG(const AdjList& G, const std::vector<int32_t>& C) {
    for (std::size_t i = 0; i < C.size(); ++i) {
        for (std::size_t j = i + 1; j < C.size(); ++j) {
            const auto& nbrs = G[static_cast<std::size_t>(C[i])];
            if (std::find(nbrs.begin(), nbrs.end(), C[j]) == nbrs.end()) return false;
        }
    }
    return true;
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

    std::vector<int32_t> clique;
    const double phi = 0.5;

    EXPECT_FALSE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/2, phi, clique));
}

TEST(NeighborSearchFilter1, NTooSmallPrunesSearch) {
    AdjList G(3);
    add_undirected_edge(G, 0, 1);
    add_undirected_edge(G, 1, 2);

    std::vector<int32_t> core_H = {3, 3, 3};
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> clique;
    const double phi = 0.5;

    EXPECT_FALSE(NeighborSearch(H, /*vH=*/1, core_H, /*incumbent_size=*/3, phi, clique));
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

    std::vector<int32_t> clique;
    const double phi = 0.5;

    EXPECT_FALSE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/3, phi, clique));
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

    std::vector<int32_t> clique;
    const double phi = 0.9;

    ASSERT_TRUE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/2, phi, clique));
    EXPECT_EQ(clique.size(), 4u);
    EXPECT_TRUE(IsCliqueInG(G, clique));
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

    std::vector<int32_t> clique;
    const double phi = 0.5;

    ASSERT_TRUE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/3, phi, clique));
    EXPECT_EQ(clique.size(), 4u);
    EXPECT_TRUE(IsCliqueInG(G, clique));
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

    std::vector<int32_t> clique;
    const double phi = 0.1;

    ASSERT_TRUE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/2, phi, clique));
    EXPECT_EQ(clique.size(), 4u);
    EXPECT_TRUE(IsCliqueInG(G, clique));
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

    std::vector<int32_t> clique;
    const double phi = 1.0; // no neighborhood is denser than 1.0

    ASSERT_TRUE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/2, phi, clique));
    EXPECT_EQ(clique.size(), 4u);
    EXPECT_TRUE(IsCliqueInG(G, clique));
}

// ---------------------------------------------------------------------------
// The dense path must return a *maximum* clique of the neighborhood, not just
// any clique that beats the incumbent: a vertex is searched only once, so a
// larger clique left behind here is never found later.
//
// N+(0) = {1..13}. In the complement of G[N]: 1 is adjacent to 2..5 and each
// of 2..5 forms a triangle with two private vertices. The minimum vertex cover
// of that complement has 8 vertices (two per triangle) and leaves the clique
// {1, 6, 7, 8, 9}; taking vertex 1 first gives a cover of 9 and only a clique
// of size 4. With vertex 0 the maximum clique therefore has 6 vertices.
// ---------------------------------------------------------------------------

namespace {

AdjList DenseNeighborhoodWithLeftoverClique() {
    std::vector<std::vector<bool>> comp(14, std::vector<bool>(14, false));
    auto add_comp = [&](int a, int b) { comp[a][b] = comp[b][a] = true; };
    for (int i = 0; i < 4; ++i) {
        const int u = 2 + i, p = 6 + i, q = 10 + i;
        add_comp(1, u);
        add_comp(u, p);
        add_comp(u, q);
        add_comp(p, q);
    }

    AdjList G(14);
    for (int32_t u = 1; u < 14; ++u) add_undirected_edge(G, 0, u);
    for (int32_t a = 1; a < 14; ++a) {
        for (int32_t b = a + 1; b < 14; ++b) {
            if (!comp[a][b]) add_undirected_edge(G, a, b);
        }
    }
    return G;
}

} // namespace

TEST(NeighborSearchDensity, DenseCaseFindsMaximumNotJustImprovingClique) {
    const AdjList G = DenseNeighborhoodWithLeftoverClique();
    std::vector<int32_t> core_H(G.size(), 20);

    for (int32_t incumbent : {3, 4, 5}) {
        auto H = MakeIdentityLazyGraph(G, core_H);
        std::vector<int32_t> clique;
        ASSERT_TRUE(NeighborSearch(H, /*vH=*/0, core_H, incumbent, /*phi=*/0.7, clique))
            << "incumbent=" << incumbent;
        EXPECT_EQ(clique.size(), 6u) << "incumbent=" << incumbent;
        EXPECT_TRUE(IsCliqueInG(G, clique));
    }
}

TEST(NeighborSearchDensity, NoImprovementWhenIncumbentIsAlreadyMaximum) {
    const AdjList G = DenseNeighborhoodWithLeftoverClique();
    std::vector<int32_t> core_H(G.size(), 20);
    auto H = MakeIdentityLazyGraph(G, core_H);

    std::vector<int32_t> clique;
    EXPECT_FALSE(NeighborSearch(H, /*vH=*/0, core_H, /*incumbent_size=*/6, /*phi=*/0.7, clique));
}
