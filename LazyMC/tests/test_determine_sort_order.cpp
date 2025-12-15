#include <gtest/gtest.h>

#include "lazymc/determine_sort_order.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using lazymc::AdjList;
using lazymc::DetermineSortOrder;

TEST(DetermineSortOrderTest, EmptyGraph) {
    AdjList G;
    std::vector<int32_t> core;

    auto order = DetermineSortOrder(G, core);

    EXPECT_TRUE(order.relabel_to_original.empty());
    EXPECT_TRUE(order.original_to_relabel.empty());
}

TEST(DetermineSortOrderTest, PermutationAndInverseMapping) {
    // Graph structure is not important for ordering logic; only size matters.
    AdjList G = {
        {1},     // 0
        {0, 2},  // 1
        {1},     // 2
        {},      // 3
        {1}      // 4
    };

    std::vector<int32_t> core = {1, 3, 3, 0, 2};

    auto order = DetermineSortOrder(G, core);

    const std::size_t n = G.size();
    ASSERT_EQ(order.relabel_to_original.size(), n);
    ASSERT_EQ(order.original_to_relabel.size(), n);

    // relabel_to_original must be a permutation of 0..n-1
    std::vector<bool> seen(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        int32_t vG = order.relabel_to_original[i];
        ASSERT_GE(vG, 0);
        ASSERT_LT(static_cast<std::size_t>(vG), n);
        EXPECT_FALSE(seen[static_cast<std::size_t>(vG)]);
        seen[static_cast<std::size_t>(vG)] = true;
    }
    for (std::size_t v = 0; v < n; ++v) {
        EXPECT_TRUE(seen[v]);
    }

    // original_to_relabel must be the inverse mapping
    for (std::size_t vG = 0; vG < n; ++vG) {
        int32_t vH = order.original_to_relabel[vG];
        ASSERT_GE(vH, 0);
        ASSERT_LT(static_cast<std::size_t>(vH), n);
        EXPECT_EQ(order.relabel_to_original[static_cast<std::size_t>(vH)],
                  static_cast<int32_t>(vG));
    }
}

TEST(DetermineSortOrderTest, NonIncreasingCoreOrder) {
    AdjList G = {
        {1}, {0, 2}, {1, 3}, {2}, {}
    };
    std::vector<int32_t> core = {1, 3, 3, 0, 2};

    auto order = DetermineSortOrder(G, core);

    // Check: core values are non-increasing along relabel_to_original
    for (std::size_t i = 1; i < order.relabel_to_original.size(); ++i) {
        int32_t prev = order.relabel_to_original[i - 1];
        int32_t curr = order.relabel_to_original[i];
        EXPECT_GE(core[static_cast<std::size_t>(prev)],
                  core[static_cast<std::size_t>(curr)]);
    }

    // Deterministic tie-breaker expectation:
    // If core is equal, vertices should appear in increasing original index order.
    for (std::size_t i = 1; i < order.relabel_to_original.size(); ++i) {
        int32_t prev = order.relabel_to_original[i - 1];
        int32_t curr = order.relabel_to_original[i];
        if (core[static_cast<std::size_t>(prev)] == core[static_cast<std::size_t>(curr)]) {
            EXPECT_LT(prev, curr);
        }
    }
}

TEST(DetermineSortOrderTest, AllZeroCoreGivesIdentityOrder) {
    AdjList G = {
        {1}, {0}, {3}, {2}
    };
    std::vector<int32_t> core = {0, 0, 0, 0};

    auto order = DetermineSortOrder(G, core);

    ASSERT_EQ(order.relabel_to_original.size(), 4u);
    ASSERT_EQ(order.original_to_relabel.size(), 4u);

    // With all ties, deterministic order should be 0..n-1
    EXPECT_EQ(order.relabel_to_original[0], 0);
    EXPECT_EQ(order.relabel_to_original[1], 1);
    EXPECT_EQ(order.relabel_to_original[2], 2);
    EXPECT_EQ(order.relabel_to_original[3], 3);

    for (std::size_t v = 0; v < 4; ++v) {
        EXPECT_EQ(order.original_to_relabel[v], static_cast<int32_t>(v));
    }
}

TEST(DetermineSortOrderTest, DeterministicAcrossCalls) {
    AdjList G = {
        {1}, {0, 2}, {1}, {}
    };
    std::vector<int32_t> core = {2, 2, 1, 0};

    auto a = DetermineSortOrder(G, core);
    auto b = DetermineSortOrder(G, core);

    EXPECT_EQ(a.relabel_to_original, b.relabel_to_original);
    EXPECT_EQ(a.original_to_relabel, b.original_to_relabel);
}
