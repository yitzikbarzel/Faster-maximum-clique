#include "lazymc/k_vertex_cover.h"
#include <vector>
#include <cstdint>
namespace lazymc {

// no self loops
// undirected
// no duplicate

k_vertex_cover_res
k_vertex_cover_adj(const AdjList& G, int32_t k) {
    // TODO: Implement k-vertex-cover pruning check
  k_vertex_cover_res result;
  result.found = false;
}
} // namespace lazymc
