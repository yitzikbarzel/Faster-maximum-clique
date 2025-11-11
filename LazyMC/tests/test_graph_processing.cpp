#include <gtest/gtest.h>
#include "lazymc/graph_processing.h"
#include <map>
#include <utility>
#include <vector>

TEST(AdjacencyListTest, EmptyGraph) {
    std::vector<std::pair<int, int>> edges = {};
    std::map<int, std::vector<int>> result = graph_processing(edges);
    EXPECT_TRUE(result.empty());
}

TEST(AdjacencyListTest, TwoEdgesFromSameNode_Undirected) {
    std::vector<std::pair<int, int>> edges = {{5, 8}, {5, 3}};
    std::map<int, std::vector<int>> result = graph_processing(edges);

    EXPECT_EQ((std::vector<int>{3, 8}), result[5]); // סוגריים כפולים בגלל המאקרו
    EXPECT_EQ((std::vector<int>{5}),     result[8]);
    EXPECT_EQ((std::vector<int>{5}),     result[3]);
}

TEST(AdjacencyListTest, DuplicateEdges_Undirected) {
    std::vector<std::pair<int, int>> edges = {{1, 2}, {1, 2}};
    std::map<int, std::vector<int>> result = graph_processing(edges);

    EXPECT_EQ((std::vector<int>{2}), result[1]);
    EXPECT_EQ((std::vector<int>{1}), result[2]);
}

TEST(AdjacencyListTest, SelfLoop_Ignored) {
    std::vector<std::pair<int, int>> edges = {{4, 4}};
    std::map<int, std::vector<int>> result = graph_processing(edges);

    if (result.count(4))
        EXPECT_TRUE(result[4].empty());
    else
        SUCCEED();
}
