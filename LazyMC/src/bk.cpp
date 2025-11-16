// #include "lazymc/bk.h"

// #include <algorithm> // std::find, std::min
// #include <vector>
// #include <map>

// namespace lazymc {

// namespace {

// // Helper: return neighbors of v if exist, otherwise an empty vector we pass in.
// const std::vector<int>& neighbors_or_empty(const AdjMap& G,
//                                            int v,
//                                            const std::vector<int>& empty) {
//     auto it = G.find(v);
//     if (it == G.end()) {
//         return empty;
//     }
//     return it->second;
// }

// // Helper: check if vec contains x (linear search).
// bool contains(const std::vector<int>& vec, int x) {
//     return std::find(vec.begin(), vec.end(), x) != vec.end();
// }

// // Helper: intersection of two vectors, using contains() on the second.
// std::vector<int> intersect_vec(const std::vector<int>& a,
//                                const std::vector<int>& b) {
//     std::vector<int> out;
//     out.reserve(std::min(a.size(), b.size()));
//     for (int x : a) {
//         if (contains(b, x)) {
//             out.push_back(x);
//         }
//     }
//     return out;
// }

// // Recursive Bron–Kerbosch for enumerating all maximal cliques (basic BK1).
// void bron_kerbosch_recursive(const AdjMap& G,
//                              std::vector<int>& R,
//                              std::vector<int>& P,
//                              std::vector<int>& X,
//                              std::vector<std::vector<int>>& cliques) {
//     if (P.empty() && X.empty()) {
//         cliques.push_back(R);
//         return;
//     }

//     std::vector<int> P_copy = P;
//     std::vector<int> empty_neighbors;

//     for (int v : P_copy) {
//         R.push_back(v);

//         const auto& Nv = neighbors_or_empty(G, v, empty_neighbors);
//         std::vector<int> P_next = intersect_vec(P, Nv);
//         std::vector<int> X_next = intersect_vec(X, Nv);

//         bron_kerbosch_recursive(G, R, P_next, X_next, cliques);

//         R.pop_back();

//         auto itP = std::find(P.begin(), P.end(), v);
//         if (itP != P.end()) {
//             P.erase(itP);
//         }
//         X.push_back(v);
//     }
// }

// // ---- Tomita-style helpers (coloring bound) ----

// // Check whether u and v are adjacent in G.
// bool is_adjacent(const AdjMap& G, int u, int v,
//                  const std::vector<int>& empty_neighbors) {
//     const auto& Nu = neighbors_or_empty(G, u, empty_neighbors);
//     return contains(Nu, v);
// }

// // Greedy coloring and ordering of candidate vertices (Tomita-style).
// // Input:
// //   G   - graph
// //   P   - current candidate set
// // Output:
// //   order  - vertices of P, ordered by non-decreasing color
// //   color  - same size as order; color[i] is the color assigned to order[i]
// void color_sort(const AdjMap& G,
//                 const std::vector<int>& P,
//                 std::vector<int>& order,
//                 std::vector<int>& color) {
//     order.clear();
//     color.clear();

//     if (P.empty()) {
//         return;
//     }

//     std::vector<int> remaining = P;
//     std::vector<int> empty_neighbors; // for neighbors_or_empty / is_adjacent
//     int current_color = 0;

//     while (!remaining.empty()) {
//         ++current_color;
//         std::vector<int> this_color_vertices;
//         std::vector<int> next_remaining;

//         for (int v : remaining) {
//             bool conflict = false;
//             for (int u : this_color_vertices) {
//                 if (is_adjacent(G, v, u, empty_neighbors)) {
//                     conflict = true;
//                     break;
//                 }
//             }

//             if (!conflict) {
//                 this_color_vertices.push_back(v);
//                 order.push_back(v);
//                 color.push_back(current_color);
//             } else {
//                 next_remaining.push_back(v);
//             }
//         }

//         remaining.swap(next_remaining);
//     }
// }

// // Tomita-style expansion with coloring bound.
// // R - current clique
// // P - candidate vertices (subset of V \ R)
// // best - best clique found so far
// void tomita_expand(const AdjMap& G,
//                    std::vector<int>& R,
//                    std::vector<int>& P,
//                    std::vector<int>& best) {
//     if (P.empty()) {
//         if (R.size() > best.size()) {
//             best = R;
//         }
//         return;
//     }

//     std::vector<int> order;
//     std::vector<int> color;
//     color_sort(G, P, order, color);

//     std::vector<int> empty_neighbors;
//     const int n = static_cast<int>(order.size());

//     // Traverse in reverse order: vertices with larger color first.
//     for (int i = n - 1; i >= 0; --i) {
//         int v = order[i];

//         // Bounding: if |R| + color[i] <= |best|, we cannot improve.
//         if (R.size() + color[i] <= best.size()) {
//             return; // smaller colors won't help either
//         }

//         const auto& Nv = neighbors_or_empty(G, v, empty_neighbors);
//         std::vector<int> P_next = intersect_vec(P, Nv);

//         R.push_back(v);

//         if (P_next.empty()) {
//             if (R.size() > best.size()) {
//                 best = R;
//             }
//         } else {
//             tomita_expand(G, R, P_next, best);
//         }

//         R.pop_back();

//         auto it = std::find(P.begin(), P.end(), v);
//         if (it != P.end()) {
//             P.erase(it);
//         }
//     }
// }

// } // anonymous namespace

// // ================== Public API ==================

// // Enumerate all maximal cliques (basic BK1, no coloring).
// std::vector<std::vector<int>> bron_kerbosch_all(const AdjMap& G) {
//     std::vector<std::vector<int>> cliques;

//     std::vector<int> P;
//     P.reserve(G.size());
//     for (const auto& [v, _] : G) {
//         P.push_back(v);
//     }

//     std::vector<int> R; // current clique
//     std::vector<int> X; // excluded vertices

//     bron_kerbosch_recursive(G, R, P, X, cliques);
//     return cliques;
// }

// // Find one maximum clique using Tomita + coloring bound.
// std::vector<int> bron_kerbosch_maximum(const AdjMap& G) {
//     std::vector<int> best;

//     if (G.empty()) {
//         return best; // no vertices, no clique
//     }

//     std::vector<int> P;
//     P.reserve(G.size());
//     for (const auto& [v, _] : G) {
//         P.push_back(v);
//     }

//     std::vector<int> R; // current clique

//     tomita_expand(G, R, P, best);
//     return best;
// }

// } // namespace lazymc
#include "lazymc/bk.h"

#include <algorithm> // std::find, std::binary_search, std::min
#include <vector>
#include <map>

namespace lazymc {

namespace {

// Return neighbors of v if exist, otherwise an empty vector we pass in.
const std::vector<int>& neighbors_or_empty(const AdjMap& G,
                                           int v,
                                           const std::vector<int>& empty) {
    auto it = G.find(v);
    if (it == G.end()) {
        return empty;
    }
    return it->second;
}

// Assuming vec is sorted: check if it contains x using binary search.
bool contains_sorted(const std::vector<int>& vec, int x) {
    return std::binary_search(vec.begin(), vec.end(), x);
}

// Intersection of two *sorted* vectors a and b, in O(|a| + |b|).
std::vector<int> intersect_sorted(const std::vector<int>& a,
                                  const std::vector<int>& b) {
    std::vector<int> out;
    out.reserve(std::min(a.size(), b.size()));

    auto itA = a.begin();
    auto itB = b.begin();

    while (itA != a.end() && itB != b.end()) {
        if (*itA < *itB) {
            ++itA;
        } else if (*itB < *itA) {
            ++itB;
        } else { // equal
            out.push_back(*itA);
            ++itA;
            ++itB;
        }
    }
    return out;
}

// ========================= BK1: all maximal cliques =========================

void bron_kerbosch_recursive_all(const AdjMap& G,
                                 std::vector<int>& R,
                                 std::vector<int>& P,
                                 std::vector<int>& X,
                                 std::vector<std::vector<int>>& cliques) {
    if (P.empty() && X.empty()) {
        cliques.push_back(R);
        return;
    }

    std::vector<int> empty_neighbors;
    std::vector<int> P_copy = P; // we may modify P, so iterate on a copy

    for (int v : P_copy) {
        R.push_back(v);

        const auto& Nv = neighbors_or_empty(G, v, empty_neighbors);
        std::vector<int> P_next = intersect_sorted(P, Nv);
        std::vector<int> X_next = intersect_sorted(X, Nv);

        bron_kerbosch_recursive_all(G, R, P_next, X_next, cliques);

        R.pop_back();

        auto itP = std::find(P.begin(), P.end(), v);
        if (itP != P.end()) {
            P.erase(itP);
        }
        X.push_back(v);
    }
}

// ===================== BK2: maximum clique with pivot =======================

// Choose pivot u from P ∪ X that maximises |N(u) ∩ P|.
int choose_pivot(const AdjMap& G,
                 const std::vector<int>& P,
                 const std::vector<int>& X) {
    int pivot = -1;
    std::size_t max_deg = 0;

    std::vector<int> empty_neighbors;

    auto consider_set = [&](const std::vector<int>& S) {
        for (int u : S) {
            const auto& Nu = neighbors_or_empty(G, u, empty_neighbors);
            std::size_t cnt = 0;
            for (int v : P) {
                if (contains_sorted(Nu, v)) {
                    ++cnt;
                }
            }
            if (cnt > max_deg) {
                max_deg = cnt;
                pivot = u;
            }
        }
    };

    consider_set(P);
    consider_set(X);

    return pivot;
}

// Bron–Kerbosch with pivot heuristic, searching only for a maximum clique.
void bron_kerbosch_recursive_pivot_max(const AdjMap& G,
                                       std::vector<int>& R,
                                       std::vector<int>& P,
                                       std::vector<int>& X,
                                       std::vector<int>& best) {
    // Simple bound: even if we add all of P to R, can't beat best.
    if (R.size() + P.size() <= best.size()) {
        return;
    }

    if (P.empty() && X.empty()) {
        if (R.size() > best.size()) {
            best = R;
        }
        return;
    }

    int pivot = choose_pivot(G, P, X);

    std::vector<int> empty_neighbors;
    const auto& Nu_pivot = (pivot == -1)
        ? empty_neighbors
        : neighbors_or_empty(G, pivot, empty_neighbors);

    // Candidates: P \ N(pivot)
    std::vector<int> candidates;
    candidates.reserve(P.size());
    if (pivot == -1) {
        candidates = P; // no pivot chosen, fall back to plain P
    } else {
        for (int v : P) {
            if (!contains_sorted(Nu_pivot, v)) {
                candidates.push_back(v);
            }
        }
    }

    // Iterate over a copy, but we will update P and X along the way.
    std::vector<int> candidates_copy = candidates;

    for (int v : candidates_copy) {
        R.push_back(v);

        const auto& Nv = neighbors_or_empty(G, v, empty_neighbors);
        std::vector<int> P_next = intersect_sorted(P, Nv);
        std::vector<int> X_next = intersect_sorted(X, Nv);

        bron_kerbosch_recursive_pivot_max(G, R, P_next, X_next, best);

        R.pop_back();

        // Move v from P to X
        auto itP = std::find(P.begin(), P.end(), v);
        if (itP != P.end()) {
            P.erase(itP);
        }
        X.push_back(v);
    }
}

} // anonymous namespace

// ========================== Public API: all cliques =========================

std::vector<std::vector<int>> bron_kerbosch_all(const AdjMap& G) {
    std::vector<std::vector<int>> cliques;

    std::vector<int> P;
    P.reserve(G.size());
    for (const auto& [v, _] : G) {
        P.push_back(v);
    }

    // נוודא ש-P ממוינת, כדי שהחיתוכים יעבדו ביעילות
    std::sort(P.begin(), P.end());

    std::vector<int> R; // current clique
    std::vector<int> X; // excluded vertices

    bron_kerbosch_recursive_all(G, R, P, X, cliques);
    return cliques;
}

// ======================= Public API: maximum clique ========================

std::vector<int> bron_kerbosch_maximum(const AdjMap& G) {
    std::vector<int> best;

    if (G.empty()) {
        return best; // no vertices, no clique
    }

    std::vector<int> P;
    P.reserve(G.size());
    for (const auto& [v, _] : G) {
        P.push_back(v);
    }

    std::sort(P.begin(), P.end());

    std::vector<int> R; // current clique
    std::vector<int> X; // excluded vertices

    bron_kerbosch_recursive_pivot_max(G, R, P, X, best);
    return best;
}

} // namespace lazymc
