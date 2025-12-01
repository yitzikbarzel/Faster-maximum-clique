#include <vector>
#include <cstdint>
namespace lazymc {
// std::vector<int32_t> neighbor_search_right(const std::vector<int32_t>& N,
//                                            const std::vector<std::vector<int32_t>>& /*G*/) {
//   // TODO: Implement right-neighborhood filtering
//   (void)N; // silence unused warning
//   return {};
// }
void NeighborSearch(const std::vector<std::vector<int32_t>> &G,
   int32_t v, const std::vector<int32_t> &core,const std::vector<int32_t> &reorder,
    int32_t &best_clique_size, double phi){
   //Filter 1 ----- N ← {u ∈ N⁺(v) : c[u] ≥ |C*|}
   std::vector<int32_t> N;
   for (int32_t u : G[v]){
    if (reorder[u] > reorder[v]){
      if (core[u] >= best_clique_size){
      N.push_back(u);
      }
     }
    }
    if (N.size() < best_clique_size) return;
    // filter 2
    std::vector<int32_t> tempN;
    for (int32_t u : N){
      if (intersect_size_gt_bool(G[u], N, best_clique_size - 2)) {
    tempN.push_back(u);
    }

    }
    N = tempN;
    if (N.size() < best_clique_size) return;
    // filter 3
    int64_t count_edge = 0;
    std::vector<int32_t> tempN2;
    for (int32_t u : N){
     int32_t d = intersect_size_gt_val(G[u], N, best_clique_size - 2);
      if (d > best_clique_size - 2) tempN2.push_back(u);
      count_edge += d;
    }
    N = tempN2;
    if (N.size() < best_clique_size) return;

    // if (count_edge/ N.size() * N.size()-1 > phi){
    //   k_vertex_Cover(G[N],N + 1 − |C∗|)
    // }else{
    //   MaximumCliqueSearch(G[N],{v})
    // }
    

  }

} // namespace lazymc
