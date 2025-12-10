#include <gtest/gtest.h>
#include "lazymc/lazygraph.h"

using lazymc::AdjList;
using lazymc::LazyGraph;

// ------------------------------------------------------------
// Test 1: Basic construction and simple mapping
// ------------------------------------------------------------

TEST(LazyGraphBasic, ConstructionAndAccessors) {
    // Simple original graph:
    // 0: 1,2
    // 1: 0
    // 2: 0
    AdjList G = {
        {1, 2}, // 0
        {0},    // 1
        {0}     // 2
    };

    // Identity-like mapping:
    // H = G (0 -> 0, 1 -> 1, 2 -> 2)
    std::vector<int32_t> r2o = {0, 1, 2}; // H -> G
    std::vector<int32_t> o2r = {0, 1, 2}; // G -> H

    // Coreness in H-labeling
    std::vector<int32_t> core = {3, 2, 1};

    LazyGraph H(G, r2o, o2r, core);

    // Basic size and graph pointer
    EXPECT_EQ(H.num_vertices(), 3u);
    EXPECT_EQ(&H.original_graph(), &G);

    // Mapping H -> G
    EXPECT_EQ(H.to_original(0), 0);
    EXPECT_EQ(H.to_original(1), 1);
    EXPECT_EQ(H.to_original(2), 2);

    // Mapping G -> H
    EXPECT_EQ(H.to_relabelled(0), 0);
    EXPECT_EQ(H.to_relabelled(1), 1);
    EXPECT_EQ(H.to_relabelled(2), 2);

    // Coreness preserved
    const auto& core_vec = H.core_values();
    ASSERT_EQ(core_vec.size(), 3u);
    EXPECT_EQ(core_vec[0], 3);
    EXPECT_EQ(core_vec[1], 2);
    EXPECT_EQ(core_vec[2], 1);

    // Hashed/sorted neighborhoods should exist and be empty at this stage
    for (int32_t v = 0; v < 3; ++v) {
        auto& hv = H.hashed_neighbors(v);
        auto const& sv = H.sorted_neighbors(v);

        EXPECT_TRUE(hv.empty());
        EXPECT_TRUE(sv.empty());
    }
}

// ------------------------------------------------------------
// Test 2: Non-trivial permutation and per-vertex containers
// ------------------------------------------------------------

TEST(LazyGraphBasic, PermutationMappingAndContainers) {
    // Original graph G:
    //
    // 0: 1,2
    // 1: 0,3
    // 2: 0,3,4
    // 3: 1,2
    // 4: 2
    AdjList G = {
        {1, 2},      // 0
        {0, 3},      // 1
        {0, 3, 4},   // 2
        {1, 2},      // 3
        {2}          // 4
    };

    // Reordering (H-labeling):
    //
    // H: 0  1  2  3  4
    // G: 2  0  3  4  1
    //
    // So:
    //   H=0 -> G=2
    //   H=1 -> G=0
    //   H=2 -> G=3
    //   H=3 -> G=4
    //   H=4 -> G=1
    std::vector<int32_t> r2o = {2, 0, 3, 4, 1}; // H -> G
    std::vector<int32_t> o2r = {1, 4, 0, 2, 3}; // G -> H

    // Coreness in H-labeling (arbitrary, but consistent)
    std::vector<int32_t> core = {
        5, // H=0
        4, // H=1
        3, // H=2
        2, // H=3
        1  // H=4
    };

    LazyGraph H(G, r2o, o2r, core);

    // Size and original graph pointer
    EXPECT_EQ(H.num_vertices(), 5u);
    EXPECT_EQ(&H.original_graph(), &G);

    // H -> G mapping
    EXPECT_EQ(H.to_original(0), 2);
    EXPECT_EQ(H.to_original(1), 0);
    EXPECT_EQ(H.to_original(2), 3);
    EXPECT_EQ(H.to_original(3), 4);
    EXPECT_EQ(H.to_original(4), 1);

    // G -> H mapping
    EXPECT_EQ(H.to_relabelled(0), 1);
    EXPECT_EQ(H.to_relabelled(1), 4);
    EXPECT_EQ(H.to_relabelled(2), 0);
    EXPECT_EQ(H.to_relabelled(3), 2);
    EXPECT_EQ(H.to_relabelled(4), 3);

    // Coreness preserved
    const auto& core_vec = H.core_values();
    ASSERT_EQ(core_vec.size(), 5u);
    EXPECT_EQ(core_vec[0], 5);
    EXPECT_EQ(core_vec[1], 4);
    EXPECT_EQ(core_vec[2], 3);
    EXPECT_EQ(core_vec[3], 2);
    EXPECT_EQ(core_vec[4], 1);

    // At this stage (before Algorithm 2 is implemented), neighborhoods
    // are empty but containers must be valid and separate per vertex.
    for (int32_t v = 0; v < 5; ++v) {
        auto& hv = H.hashed_neighbors(v);
        auto const& sv = H.sorted_neighbors(v);

        EXPECT_TRUE(hv.empty());
        EXPECT_TRUE(sv.empty());

        // Insert a marker into hashed neighbors for this vertex only
        hv.insert(100 + v);
    }

    // Verify that each vertex has its own hashed container
    for (int32_t v = 0; v < 5; ++v) {
        const auto& hv = H.hashed_neighbors(v);

        // Exactly one marker per vertex
        EXPECT_EQ(hv.size(), 1u);
        EXPECT_TRUE(hv.count(100 + v) == 1);

        // No markers from other vertices
        for (int32_t u = 0; u < 5; ++u) {
            if (u == v) continue;
            EXPECT_TRUE(hv.count(100 + u) == 0);
        }
    }
}
