#include "lazymc/degree_heuristic.h"
#include "lazymc/intersect_functions.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace lazymc {

std::vector<int32_t> degree_based_heuristic(const AdjList& G, int top_k) {
    if (G.empty()) return {};

    const int32_t n = static_cast<int32_t>(G.size());

    std::vector<int32_t> relabel_to_original(n);
    std::vector<int32_t> original_to_relabel(n);
    std::vector<int32_t> core(n, 0);

    for (int32_t i = 0; i < n; ++i) {
        relabel_to_original[i] = i;
        original_to_relabel[i] = i;
    }

    LazyGraph H(G, relabel_to_original, original_to_relabel, core);
    return degree_based_heuristic(H, top_k);
}

std::vector<int32_t> degree_based_heuristic(LazyGraph& H, int top_k) {
    const int32_t n = static_cast<int32_t>(H.num_vertices());
    if (n <= 0) return {};

    std::vector<int32_t> best_clique;

    // degree ordering (ORIGINAL)
    std::vector<std::pair<int32_t,int32_t>> degree_vertex;
    degree_vertex.reserve(static_cast<std::size_t>(n));
    for (int32_t v = 0; v < n; ++v) {
        degree_vertex.push_back(
            {static_cast<int32_t>(H.sorted_neighbors(v).size()), v});
    }

    std::sort(degree_vertex.begin(), degree_vertex.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    const int32_t k = std::min<int32_t>(top_k, n);

    std::vector<int32_t> tmp;

    // NEW 2: marking infra
    std::vector<int32_t> mark(static_cast<std::size_t>(n), 0);
    int32_t stamp = 0;

    for (int32_t i = 0; i < k; ++i) {
        const int32_t v = degree_vertex[static_cast<std::size_t>(i)].second;

        std::vector<int32_t> N;
        const auto& Nv = H.sorted_neighbors(v);
        N.reserve(Nv.size());

        for (int32_t u : Nv) {
            if (static_cast<int32_t>(H.sorted_neighbors(u).size()) >=
                static_cast<int32_t>(best_clique.size())) {
                N.push_back(u);
            }
        }

        std::vector<int32_t> C;
        C.push_back(v);

        while (!N.empty()) {

            // NEW 7: prune by candidate size
            // If even taking all remaining candidates can't beat best_clique, stop.
            if (static_cast<int32_t>(N.size()) <=
                static_cast<int32_t>(best_clique.size())) {
                break;
            }

            // NEW 2: mark N
            ++stamp;
            for (int32_t x : N) {
                mark[static_cast<std::size_t>(x)] = stamp;
            }

            // NEW 2: choose best_u by counting marked neighbors
            // + NEW 5: early-exit while counting
            int32_t best_u = -1;
            int32_t best_score = -1;

            for (int32_t w : N) {
                int32_t score = 0;

                const auto& neigh = H.sorted_neighbors(w);
                int32_t remaining = static_cast<int32_t>(neigh.size());

                for (int32_t u : neigh) {
                    --remaining;

                    if (mark[static_cast<std::size_t>(u)] == stamp) {
                        ++score;
                        if (score > best_score) {
                            break; // already beats best_score
                        }
                    }

                    // NEW 5: cannot beat best_score anymore
                    if (score + remaining <= best_score) {
                        break;
                    }
                }

                if (score > best_score) {
                    best_score = score;
                    best_u = w;
                }
            }

            if (best_u < 0) break;

            C.push_back(best_u);

            // ORIGINAL update step (unchanged):
            const auto& Bu = H.hashed_neighbors(best_u);
            int32_t d = intersect_gt(N, Bu, tmp, 0);

            if (d <= 0) {
                N.clear();
            } else {
                N.assign(tmp.begin(), tmp.begin() + d);
                N.erase(std::remove(N.begin(), N.end(), best_u), N.end());
            }
        }

        if (C.size() > best_clique.size()) {
            best_clique = std::move(C);
        }
    }

    return best_clique;
}

} // namespace lazymc
