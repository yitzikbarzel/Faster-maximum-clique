#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

// Forward declaration
namespace lazymc {
    std::vector<int32_t> degree_based_heuristic(
        const std::vector<std::vector<int32_t>>& G, 
        int top_k);
}

TEST(DegreeBasedHeuristicTest, EmptyGraph) {
    std::vector<std::vector<int32_t>> G;
    auto result = lazymc::degree_based_heuristic(G, 5);
    EXPECT_TRUE(result.empty());
}

TEST(DegreeBasedHeuristicTest, SingleVertex) {
    std::vector<std::vector<int32_t>> G = {{}};
    auto result = lazymc::degree_based_heuristic(G, 5);
    EXPECT_EQ(result.size(), 1);
}

TEST(DegreeBasedHeuristicTest, Triangle) {
    // Triangle: 0-1-2-0
    std::vector<std::vector<int32_t>> G = {
        {1, 2},  // vertex 0 neighbors
        {0, 2},  // vertex 1 neighbors
        {0, 1}   // vertex 2 neighbors
    };
    auto result = lazymc::degree_based_heuristic(G, 3);
    EXPECT_EQ(result.size(), 3);  // Should find the triangle clique
}

TEST(DegreeBasedHeuristicTest, K4Clique) {
    // Complete graph K4: all vertices connected
    std::vector<std::vector<int32_t>> G = {
        {1, 2, 3},  // vertex 0
        {0, 2, 3},  // vertex 1
        {0, 1, 3},  // vertex 2
        {0, 1, 2}   // vertex 3
    };
    auto result = lazymc::degree_based_heuristic(G, 4);
    EXPECT_EQ(result.size(), 4);  // Should find the complete K4 clique
}
