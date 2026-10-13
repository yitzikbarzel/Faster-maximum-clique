#include "lazymc/bk.h"
#include "lazymc/clique_solver.h"

#include <algorithm> // std::find, std::sort, std::min
#include <unordered_map>
#include <vector>

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

// Copy of G with every neighbor list sorted and deduplicated, as the
// intersections above require.
AdjMap sorted_copy(const AdjMap& G) {
    AdjMap out = G;
    for (auto& [v, nbrs] : out) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }
    return out;
}

} // anonymous namespace

// ========================== Public API: all cliques =========================

std::vector<std::vector<int>> bron_kerbosch_all(const AdjMap& input) {
    std::vector<std::vector<int>> cliques;
    const AdjMap G = sorted_copy(input);

    std::vector<int> P;
    P.reserve(G.size());
    for (const auto& [v, _] : G) {
        P.push_back(v);
    }

    // Keep P sorted so the intersections work.
    std::sort(P.begin(), P.end());

    std::vector<int> R; // current clique
    std::vector<int> X; // excluded vertices

    bron_kerbosch_recursive_all(G, R, P, X, cliques);
    return cliques;
}

// ======================= Public API: maximum clique ========================

std::vector<int> bron_kerbosch_maximum(const AdjMap& G, int initial_best_size) {
    if (G.empty() || static_cast<int>(G.size()) <= initial_best_size) {
        return {};
    }

    // Relabel the keys to 0..n-1 and hand the graph to the exact solver.
    std::vector<int> ids;
    ids.reserve(G.size());
    for (const auto& [v, _] : G) ids.push_back(v);
    std::sort(ids.begin(), ids.end());

    std::unordered_map<int, int32_t> local;
    local.reserve(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) local.emplace(ids[i], static_cast<int32_t>(i));

    AdjList g(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (int u : G.at(ids[i])) {
            auto it = local.find(u);
            if (it == local.end() || it->second == static_cast<int32_t>(i)) continue;
            g[i].push_back(it->second);
            g[static_cast<std::size_t>(it->second)].push_back(static_cast<int32_t>(i));
        }
    }
    for (auto& nbrs : g) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }

    std::vector<int32_t> clique = max_clique_adj(g, initial_best_size);
    std::vector<int> best;
    best.reserve(clique.size());
    for (int32_t i : clique) best.push_back(ids[static_cast<std::size_t>(i)]);
    return best;
}

} // namespace lazymc
