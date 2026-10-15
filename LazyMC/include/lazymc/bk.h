#pragma once

#include <unordered_map>
#include <vector>


namespace lazymc {

/// Adjacency map type:
/// - key: vertex id (int)
/// - value: list of neighbors (each neighbor is an int as well)

using AdjMap = std::unordered_map<int, std::vector<int>>;


/// Bron–Kerbosch algorithm: enumerate all maximal cliques.
/// - Input: undirected simple graph represented as an adjacency map.
/// - Output: list of maximal cliques, each clique is a vector of vertex ids.
std::vector<std::vector<int>>
bron_kerbosch_all(const AdjMap& G);

/// Exact search for a single maximum clique.
/// - Input: same adjacency map as above (neighbor lists need not be sorted).
/// - initial_best_size: only cliques with more than this many vertices are
///   searched for.
/// - Output: a maximum clique if it has more than initial_best_size vertices,
///   otherwise an empty vector.
std::vector<int>
bron_kerbosch_maximum(const AdjMap& G, int initial_best_size = 0);

} // namespace lazymc
