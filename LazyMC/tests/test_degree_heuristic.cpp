#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "lazymc/degree_heuristic.h"
#include "lazymc/lazygraph.h"

namespace {

// Helper to build a LazyGraph with identity relabeling (H-ids == G-ids).
// IMPORTANT: The vectors must outlive LazyGraph if LazyGraph stores references/pointers to them.
struct IdentityLazyGraph {
    std::vector<int32_t> relabel_to_original;
    std::vector<int32_t> original_to_relabel;
    std::vector<int32_t> core;
    lazymc::LazyGraph H;

    explicit IdentityLazyGraph(const lazymc::AdjList& G)
        : relabel_to_original(static_cast<int32_t>(G.size())),
          original_to_relabel(static_cast<int32_t>(G.size())),
          core(static_cast<int32_t>(G.size()), 0),
          H(G, relabel_to_original, original_to_relabel, core) {
        const int32_t n = static_cast<int32_t>(G.size());
        for (int32_t i = 0; i < n; ++i) {
            relabel_to_original[i] = i;
            original_to_relabel[i] = i;
        }
    }
};

} // namespace

TEST(DegreeBasedHeuristicTest, EmptyGraph) {
    lazymc::AdjList G;
    IdentityLazyGraph ig(G);
    auto result = lazymc::degree_based_heuristic(ig.H, 5);
    EXPECT_TRUE(result.empty());
}

TEST(DegreeBasedHeuristicTest, SingleVertex) {
    lazymc::AdjList G = {{}};
    IdentityLazyGraph ig(G);
    auto result = lazymc::degree_based_heuristic(ig.H, 5);
    EXPECT_EQ(result.size(), 1);
}

TEST(DegreeBasedHeuristicTest, Triangle) {
    lazymc::AdjList G = {
        {1, 2},
        {0, 2},
        {0, 1}
    };
    IdentityLazyGraph ig(G);
    auto result = lazymc::degree_based_heuristic(ig.H, 3);
    EXPECT_EQ(result.size(), 3);
}

TEST(DegreeBasedHeuristicTest, K4Clique) {
    lazymc::AdjList G = {
        {1, 2, 3},
        {0, 2, 3},
        {0, 1, 3},
        {0, 1, 2}
    };
    IdentityLazyGraph ig(G);
    auto result = lazymc::degree_based_heuristic(ig.H, 4);
    EXPECT_EQ(result.size(), 4);
}
