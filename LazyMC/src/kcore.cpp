#include "lazymc/kcore.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lazymc {

// Core decomposition in O(n + m) using the classic bin-sort / bucket method.
// min_degree is used as a post-filter to match the paper's "if d(v) >= |C*|" condition:
// vertices whose *initial* degree is < min_degree get core[v] = 0.
std::vector<int32_t> KCore(const AdjList& G, int32_t min_degree) {
    const std::size_t n = G.size();

    if (min_degree < 0) {
        min_degree = 0;
    }

    // Compute initial degrees (and keep a copy for the min_degree post-filter).
    std::vector<int32_t> deg(n, 0);
    std::vector<int32_t> init_deg(n, 0);

    int32_t max_deg = 0;
    for (std::size_t v = 0; v < n; ++v) {
        const int32_t d = static_cast<int32_t>(G[v].size());
        deg[v] = d;
        init_deg[v] = d;
        if (d > max_deg) {
            max_deg = d;
        }
    }

    // Bin counts: bin[d] = how many vertices currently have degree d (initially).
    std::vector<std::size_t> bin(static_cast<std::size_t>(max_deg) + 1, 0);
    for (std::size_t v = 0; v < n; ++v) {
        const std::size_t d = static_cast<std::size_t>(deg[v]);
        ++bin[d];
    }

    // Prefix sums to compute bucket starting positions.
    std::vector<std::size_t> start(static_cast<std::size_t>(max_deg) + 1, 0);
    {
        std::size_t sum = 0;
        for (std::size_t d = 0; d < start.size(); ++d) {
            start[d] = sum;
            sum += bin[d];
        }
    }

    // vert: vertices ordered by nondecreasing degree (initially).
    // pos[v]: current position of vertex v inside vert.
    std::vector<int32_t> vert(n, 0);
    std::vector<std::size_t> pos(n, 0);

    // We will place vertices into vert[] using start[] as insertion cursors.
    std::vector<std::size_t> cursor = start;

    for (std::size_t v = 0; v < n; ++v) {
        const std::size_t d = static_cast<std::size_t>(deg[v]);
        const std::size_t p = cursor[d]++;
        vert[p] = static_cast<int32_t>(v);
        pos[v] = p;
    }

    // To support "move a vertex to the front of its new degree bucket",
    // we need the current bucket boundaries. We'll maintain start[] as the
    // first index of each degree bucket as degrees decrease.
    //
    // At this point:
    // - vert[] contains all vertices grouped by degree.
    // - start[d] is the first index of degree d bucket.
    // - pos[v] tells where v currently sits.

    // Core numbers: in this algorithm, core[v] becomes the final deg[v] when processed.
    std::vector<int32_t> core(n, 0);

    for (std::size_t i = 0; i < n; ++i) {
        const int32_t v_i32 = vert[i];
        const std::size_t v = static_cast<std::size_t>(v_i32);

        core[v] = deg[v];

        // For each neighbor u of v, if deg[u] > deg[v], decrement deg[u] by 1
        // and move u to the correct bucket (deg[u]-1).
        for (int32_t u_i32 : G[v]) {
            const std::size_t u = static_cast<std::size_t>(u_i32);
            if (u >= n) continue;

            if (deg[u] > deg[v]) {
                const int32_t du = deg[u];
                const std::size_t du_sz = static_cast<std::size_t>(du);

                // u is currently in the bucket of degree du. We'll move it to degree du-1.
                const std::size_t pu = pos[u];
                const std::size_t pw = start[du_sz]; // first position of the du bucket

                // Swap u with the vertex currently at the front of its bucket.
                const int32_t w_i32 = vert[pw];
                const std::size_t w = static_cast<std::size_t>(w_i32);

                if (u != w) {
                    vert[pu] = static_cast<int32_t>(w);
                    vert[pw] = static_cast<int32_t>(u);
                    pos[u] = pw;
                    pos[w] = pu;
                }

                // Advance the start of the du bucket (since u moved out of it).
                ++start[du_sz];

                // Finally decrement degree.
                deg[u] = du - 1;
            }
        }
    }

    // Post-filter to match the paper's "only if d(v) >= |C*|".
    // Use initial degree (in G) as the condition.
    if (min_degree > 0) {
        for (std::size_t v = 0; v < n; ++v) {
            if (init_deg[v] < min_degree) {
                core[v] = 0;
            }
        }
    }

    return core;
}

std::vector<int32_t> KCoreAtLeast(const AdjList& G, int32_t k) {
    if (k <= 0) return KCore(G, 0);

    const auto n = static_cast<int32_t>(G.size());
    std::vector<int32_t> core(static_cast<std::size_t>(n), 0);

    // Candidates: degree >= k. Their degree counts only candidate neighbors.
    std::vector<std::uint8_t> alive(static_cast<std::size_t>(n), 0);
    for (int32_t v = 0; v < n; ++v) {
        alive[static_cast<std::size_t>(v)] = G[static_cast<std::size_t>(v)].size() >= static_cast<std::size_t>(k);
    }
    std::vector<int32_t> deg(static_cast<std::size_t>(n), 0);
    std::vector<int32_t> queue;
    for (int32_t v = 0; v < n; ++v) {
        if (!alive[static_cast<std::size_t>(v)]) continue;
        int32_t d = 0;
        for (int32_t u : G[static_cast<std::size_t>(v)]) d += alive[static_cast<std::size_t>(u)];
        deg[static_cast<std::size_t>(v)] = d;
        if (d < k) queue.push_back(v);
    }

    // Peel down to the k-core.
    for (int32_t v : queue) alive[static_cast<std::size_t>(v)] = 0;
    for (std::size_t q = 0; q < queue.size(); ++q) {
        for (int32_t u : G[static_cast<std::size_t>(queue[q])]) {
            if (alive[static_cast<std::size_t>(u)] && --deg[static_cast<std::size_t>(u)] < k) {
                alive[static_cast<std::size_t>(u)] = 0;
                queue.push_back(u);
            }
        }
    }

    // Bin-sort core decomposition (as in KCore) restricted to the k-core.
    std::vector<int32_t> vert;
    int32_t max_deg = 0;
    for (int32_t v = 0; v < n; ++v) {
        if (alive[static_cast<std::size_t>(v)]) {
            vert.push_back(v);
            max_deg = std::max(max_deg, deg[static_cast<std::size_t>(v)]);
        }
    }
    if (vert.empty()) return core;

    std::vector<int32_t> start(static_cast<std::size_t>(max_deg) + 2, 0);
    for (int32_t v : vert) ++start[static_cast<std::size_t>(deg[static_cast<std::size_t>(v)]) + 1];
    for (std::size_t d = 1; d < start.size(); ++d) start[d] += start[d - 1];

    std::vector<int32_t> pos(static_cast<std::size_t>(n), 0);
    {
        std::vector<int32_t> cursor(start.begin(), start.end() - 1);
        std::vector<int32_t> sorted(vert.size());
        for (int32_t v : vert) {
            const auto d = static_cast<std::size_t>(deg[static_cast<std::size_t>(v)]);
            pos[static_cast<std::size_t>(v)] = cursor[d]++;
            sorted[static_cast<std::size_t>(pos[static_cast<std::size_t>(v)])] = v;
        }
        vert.swap(sorted);
    }

    for (std::size_t i = 0; i < vert.size(); ++i) {
        const int32_t v = vert[i];
        const int32_t dv = deg[static_cast<std::size_t>(v)];
        core[static_cast<std::size_t>(v)] = dv;
        for (int32_t u : G[static_cast<std::size_t>(v)]) {
            if (!alive[static_cast<std::size_t>(u)]) continue;
            const int32_t du = deg[static_cast<std::size_t>(u)];
            if (du <= dv) continue;
            const int32_t pu = pos[static_cast<std::size_t>(u)];
            const int32_t pw = start[static_cast<std::size_t>(du)];
            const int32_t w = vert[static_cast<std::size_t>(pw)];
            if (u != w) {
                vert[static_cast<std::size_t>(pu)] = w;
                vert[static_cast<std::size_t>(pw)] = u;
                pos[static_cast<std::size_t>(u)] = pw;
                pos[static_cast<std::size_t>(w)] = pu;
            }
            ++start[static_cast<std::size_t>(du)];
            deg[static_cast<std::size_t>(u)] = du - 1;
        }
    }
    return core;
}

} // namespace lazymc
