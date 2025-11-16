#include "lazymc/bk.h"

#include <gtest/gtest.h>
#include <algorithm>

using lazymc::AdjMap;
using lazymc::bron_kerbosch_all;
using lazymc::bron_kerbosch_maximum;

namespace {

// Helper: sort a clique so we can compare ignoring order
std::vector<int> sorted(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    return v;
}

} // namespace



TEST(BronKerboschTest, SingleEdge) {
    AdjMap G;
    G[0] = {1};
    G[1] = {0};

    auto all = bron_kerbosch_all(G);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(sorted(all[0]), (std::vector<int>{0, 1}));

    auto best = bron_kerbosch_maximum(G);
    EXPECT_EQ(sorted(best), (std::vector<int>{0, 1}));
}

TEST(BronKerboschTest, TriangleClique) {
    AdjMap G;
    G[0] = {1, 2};
    G[1] = {0, 2};
    G[2] = {0, 1};

    auto all = bron_kerbosch_all(G);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(sorted(all[0]), (std::vector<int>{0, 1, 2}));

    auto best = bron_kerbosch_maximum(G);
    EXPECT_EQ(sorted(best), (std::vector<int>{0, 1, 2}));
}

TEST(BronKerboschTest, TwoCliquesDifferentSizes) {
    AdjMap G;
    // Clique {0,1,2}
    G[0] = {1, 2};
    G[1] = {0, 2};
    G[2] = {0, 1};

    // Clique {3,4}
    G[3] = {4};
    G[4] = {3};

    auto all = bron_kerbosch_all(G);
    // Expect two maximal cliques: {0,1,2} and {3,4}
    ASSERT_EQ(all.size(), 2u);

    // Sort each clique and then sort list of cliques
    std::vector<std::vector<int>> cliques_sorted;
    for (auto c : all) {
        cliques_sorted.push_back(sorted(std::move(c)));
    }
    std::sort(cliques_sorted.begin(), cliques_sorted.end());

    EXPECT_EQ(cliques_sorted[0], (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(cliques_sorted[1], (std::vector<int>{3, 4}));

    auto best = bron_kerbosch_maximum(G);
    EXPECT_EQ(sorted(best), (std::vector<int>{0, 1, 2}));
}
