#include "lazymc/neighbor_search.h"

#include "lazymc/clique_solver.h"
#include "lazymc/incumbent.h"
#include "lazymc/k_vertex_cover.h"
#include "lazymc/lazygraph.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

namespace lazymc {

namespace {

// The vertex cover route only pays off when the complement of G[N] is very
// sparse. Measured on G(n, p) neighborhoods: with an average complement
// degree of ~4 it was ~200x faster than the bitset clique solver, with ~6 or
// more it was slower (up to ~30x at ~15).
constexpr double kMaxComplementDegreeForVertexCover = 5.0;

// Step budget for the vertex cover solver. If it runs out, the bitset clique
// solver finishes the job, so this only trades between two exact solvers.
constexpr long long kVertexCoverStepLimit = 1'000'000;

struct FreeDeleter {
    void operator()(void* p) const noexcept { std::free(p); }
};

// Per-thread scratch space, reused across calls so that a call costs
// O(size of the neighborhood) instead of O(|V|).
//
// The arrays have one entry per vertex of H. They are allocated with calloc
// / malloc rather than std::vector so that the pages of a large array are
// only committed (and zeroed by the OS) when touched; a thread that sees a
// few neighborhoods does not pay for zeroing the whole array.
struct Scratch {
    std::unique_ptr<uint32_t[], FreeDeleter> stamp; // stamp[u] == epoch <=> u is marked
    std::unique_ptr<int32_t[], FreeDeleter> local;  // local id of a marked vertex
    std::size_t capacity = 0;
    uint32_t epoch = 0;

    std::vector<int32_t> N;
    std::vector<int32_t> tmp;

    void ensure(std::size_t n) {
        if (capacity >= n) return;
        auto* st = static_cast<uint32_t*>(std::calloc(n, sizeof(uint32_t)));
        auto* lo = static_cast<int32_t*>(std::malloc(n * sizeof(int32_t)));
        if (!st || !lo) {
            std::free(st);
            std::free(lo);
            throw std::bad_alloc();
        }
        stamp.reset(st);
        local.reset(lo);
        capacity = n;
        epoch = 0;
    }

    uint32_t next_epoch() {
        if (++epoch == 0) {
            std::fill(stamp.get(), stamp.get() + capacity, 0u);
            epoch = 1;
        }
        return epoch;
    }
};

Scratch& scratch_for(std::size_t n) {
    thread_local Scratch s;
    s.ensure(n);
    return s;
}

// The part of the sorted neighbor array Nu that can intersect a sorted set
// whose elements lie in [lo, hi].
inline std::pair<const int32_t*, const int32_t*>
range_within(const std::vector<int32_t>& Nu, int32_t lo, int32_t hi) {
    const int32_t* b = std::lower_bound(Nu.data(), Nu.data() + Nu.size(), lo);
    const int32_t* e = std::upper_bound(b, Nu.data() + Nu.size(), hi);
    return {b, e};
}

// True iff |Nu ∩ N| >= need, where N is marked with `epoch`.
// Exits as soon as the answer is known (Algorithm 4).
bool has_enough_common(const std::vector<int32_t>& Nu,
                       const std::vector<int32_t>& N,
                       const Scratch& s, uint32_t epoch, int32_t need) {
    if (need <= 0) return true;
    auto [b, e] = range_within(Nu, N.front(), N.back());
    int32_t remaining = static_cast<int32_t>(e - b);
    if (remaining < need) return false;

    int32_t count = 0;
    for (const int32_t* p = b; p != e; ++p, --remaining) {
        if (s.stamp[static_cast<std::size_t>(*p)] == epoch) {
            if (++count >= need) return true;
        } else if (count + remaining - 1 < need) {
            return false;
        }
    }
    return false;
}

bool neighbor_search_impl(LazyGraph& H,
                          int32_t vH,
                          const std::vector<int32_t>& core_H,
                          int32_t best,
                          const std::atomic<int32_t>* shared_size,
                          double phi,
                          std::vector<int32_t>& out_clique_G) {
    const auto nH = static_cast<int32_t>(H.num_vertices());
    if (vH < 0 || vH >= nH) return false;

    Scratch& s = scratch_for(static_cast<std::size_t>(nH));
    std::vector<int32_t>& N = s.N;

    // -----------------------
    // Filter 1: N <- {u in N+(v) : c[u] >= |C*|}
    // -----------------------
    N.clear();
    {
        const auto& Nv = H.sorted_neighbors(vH);
        for (auto it = std::upper_bound(Nv.begin(), Nv.end(), vH); it != Nv.end(); ++it) {
            if (core_H[static_cast<std::size_t>(*it)] >= best) N.push_back(*it);
        }
    }
    if (static_cast<int32_t>(N.size()) < best) return false;

    // A vertex of a clique larger than |C*| that contains v has at least
    // |C*| - 1 neighbors inside N.
    const int32_t need = best - 1;

    // -----------------------
    // Filter 2 (Algorithm 4): keep u if |N(u) ∩ N| >= |C*| - 1
    // -----------------------
    if (need > 0) {
        const uint32_t epoch = s.next_epoch();
        for (int32_t u : N) s.stamp[static_cast<std::size_t>(u)] = epoch;

        s.tmp.clear();
        for (int32_t u : N) {
            if (has_enough_common(H.sorted_neighbors(u), N, s, epoch, need)) {
                s.tmp.push_back(u);
            }
        }
        N.swap(s.tmp);
        if (static_cast<int32_t>(N.size()) < best) return false;
    }

    // -----------------------
    // Filter 3: build G[N] with local ids and peel it to its (|C*|-1)-core.
    // This repeats Filter 2 until nothing changes.
    // -----------------------
    auto m = static_cast<int32_t>(N.size());
    AdjList local_adj(static_cast<std::size_t>(m));
    if (m > 0) {
        const uint32_t epoch = s.next_epoch();
        for (int32_t i = 0; i < m; ++i) {
            s.stamp[static_cast<std::size_t>(N[static_cast<std::size_t>(i)])] = epoch;
            s.local[static_cast<std::size_t>(N[static_cast<std::size_t>(i)])] = i;
        }
        for (int32_t i = 0; i < m; ++i) {
            const auto& Nu = H.sorted_neighbors(N[static_cast<std::size_t>(i)]);
            auto [b, e] = range_within(Nu, N.front(), N.back());
            auto& out = local_adj[static_cast<std::size_t>(i)];
            for (const int32_t* p = b; p != e; ++p) {
                if (s.stamp[static_cast<std::size_t>(*p)] == epoch) {
                    out.push_back(s.local[static_cast<std::size_t>(*p)]);
                }
            }
        }
    }

    if (need > 0) {
        std::vector<int32_t> deg(static_cast<std::size_t>(m));
        std::vector<int32_t> queue;
        std::vector<std::uint8_t> alive(static_cast<std::size_t>(m), 1);
        for (int32_t i = 0; i < m; ++i) {
            deg[static_cast<std::size_t>(i)] = static_cast<int32_t>(local_adj[static_cast<std::size_t>(i)].size());
            if (deg[static_cast<std::size_t>(i)] < need) {
                alive[static_cast<std::size_t>(i)] = 0;
                queue.push_back(i);
            }
        }
        for (std::size_t q = 0; q < queue.size(); ++q) {
            for (int32_t j : local_adj[static_cast<std::size_t>(queue[q])]) {
                if (alive[static_cast<std::size_t>(j)] && --deg[static_cast<std::size_t>(j)] < need) {
                    alive[static_cast<std::size_t>(j)] = 0;
                    queue.push_back(j);
                }
            }
        }

        if (!queue.empty()) {
            const int32_t m2 = m - static_cast<int32_t>(queue.size());
            if (m2 < best) return false;

            std::vector<int32_t> new_id(static_cast<std::size_t>(m), -1);
            int32_t next = 0;
            for (int32_t i = 0; i < m; ++i) {
                if (alive[static_cast<std::size_t>(i)]) {
                    new_id[static_cast<std::size_t>(i)] = next;
                    N[static_cast<std::size_t>(next)] = N[static_cast<std::size_t>(i)];
                    ++next;
                }
            }
            N.resize(static_cast<std::size_t>(m2));

            AdjList compact(static_cast<std::size_t>(m2));
            for (int32_t i = 0; i < m; ++i) {
                const int32_t ni = new_id[static_cast<std::size_t>(i)];
                if (ni < 0) continue;
                auto& out = compact[static_cast<std::size_t>(ni)];
                for (int32_t j : local_adj[static_cast<std::size_t>(i)]) {
                    const int32_t nj = new_id[static_cast<std::size_t>(j)];
                    if (nj >= 0) out.push_back(nj);
                }
            }
            local_adj.swap(compact);
            m = m2;
        }
    }

    const int32_t vG = H.to_original(vH);
    auto emit = [&](const std::vector<int32_t>& clique_local) {
        out_clique_G.clear();
        out_clique_G.reserve(clique_local.size() + 1);
        out_clique_G.push_back(vG);
        for (int32_t i : clique_local) {
            out_clique_G.push_back(H.to_original(N[static_cast<std::size_t>(i)]));
        }
        return true;
    };

    if (m == 0) {
        // Only v itself is left.
        if (best < 1) return emit({});
        return false;
    }

    int64_t twice_edges = 0;
    for (const auto& a : local_adj) twice_edges += static_cast<int64_t>(a.size());
    const double density =
        (m > 1) ? static_cast<double>(twice_edges) / (static_cast<double>(m) * (m - 1)) : 1.0;

    // A clique in N with >= |C*| vertices, plus v, beats the incumbent.
    int32_t lower_bound = best - 1;
    std::vector<int32_t> vc_clique;

    // -----------------------
    // Dense: minimum vertex cover on the complement (Section IV-E).
    // A vertex cover of size <= m - |C*| leaves a clique of size >= |C*|.
    // -----------------------
    const double complement_degree = (1.0 - density) * (m - 1);
    if (density > phi && complement_degree <= kMaxComplementDegreeForVertexCover && best > 0) {
        AdjList complement(static_cast<std::size_t>(m));
        for (int32_t i = 0; i < m; ++i) {
            const auto& a = local_adj[static_cast<std::size_t>(i)];
            auto it = a.begin();
            for (int32_t j = 0; j < m; ++j) {
                while (it != a.end() && *it < j) ++it;
                if (j != i && (it == a.end() || *it != j)) {
                    complement[static_cast<std::size_t>(i)].push_back(j);
                }
            }
        }

        min_vertex_cover_res res =
            min_vertex_cover_adj(complement, m - best, kVertexCoverStepLimit);

        if (res.found) {
            std::vector<std::uint8_t> in_cover(static_cast<std::size_t>(m), 0);
            for (int32_t i : res.cover) in_cover[static_cast<std::size_t>(i)] = 1;
            for (int32_t i = 0; i < m; ++i) {
                if (!in_cover[static_cast<std::size_t>(i)]) vc_clique.push_back(i);
            }
        }

        if (!res.aborted) {
            // Exact answer: the complement of a minimum cover is a maximum clique.
            if (!res.found) return false;
            return emit(vc_clique);
        }

        // Out of steps: let the clique solver finish, starting from what
        // the cover search already found.
        lower_bound = std::max(lower_bound, static_cast<int32_t>(vc_clique.size()));
    }

    // -----------------------
    // Sparse (or unfinished dense): exact clique search on G[N].
    // -----------------------
    std::vector<int32_t> clique = max_clique_adj(local_adj, lower_bound, shared_size, 1);
    if (!clique.empty()) return emit(clique);
    if (!vc_clique.empty()) return emit(vc_clique);
    return false;
}

} // anonymous namespace

bool NeighborSearch(LazyGraph& H,
                    int32_t vH,
                    const std::vector<int32_t>& core_H,
                    int32_t incumbent_size,
                    double phi,
                    std::vector<int32_t>& out_clique_G) {
    return neighbor_search_impl(H, vH, core_H, incumbent_size, nullptr, phi, out_clique_G);
}

bool NeighborSearch(LazyGraph& H,
                    int32_t vH,
                    const std::vector<int32_t>& core_H,
                    const Incumbent& incumbent,
                    double phi,
                    std::vector<int32_t>& out_clique_G) {
    return neighbor_search_impl(H, vH, core_H, incumbent.size(), &incumbent.size_ref(),
                                phi, out_clique_G);
}

} // namespace lazymc
