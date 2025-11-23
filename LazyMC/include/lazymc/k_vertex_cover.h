#pragma once

#include <cstdint>
#include <vector>
// Adjacency list representation for an undirected graph.
// Vertices are numbered 0..n-1.
// For each vertex v, G[v] contains the neighbors of v.
namespace lazymc {
using AdjList = std::vector<std::vector<int32_t>>;

// Result object returned by the k-Vertex-Cover solver.
// If 'found' is false, no vertex cover of size <= k exists.
// If 'found' is true, 'cover' contains a valid vertex cover.

struct k_vertex_cover_res {
  bool found = false;
  std::vector<int32_t> cover;
};

k_vertex_cover_res
k_vertex_cover_adj(const AdjList& G, int32_t k);
}