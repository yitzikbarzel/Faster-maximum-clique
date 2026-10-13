#include "lazymc/clique_solver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace lazymc {

namespace {

inline int popcount64(std::uint64_t x) {
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt64(x));
#else
    return __builtin_popcountll(x);
#endif
}

inline int ctz64(std::uint64_t x) {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
#else
    return __builtin_ctzll(x);
#endif
}

// Graphs up to this many vertices are handed to the bitset solver directly
// (128 MiB of adjacency bits at the limit). Larger ones are decomposed first.
// The decomposition rebuilds a subgraph per vertex, which is slow when the
// degeneracy is high: on an R-MAT graph (scale 20) neighborhoods of 10k-18k
// vertices took ~90 s CPU that way and ~6 s with direct bitset search.
constexpr int32_t kDirectBitsetLimit = 32768;

// Min-degree peeling (Batagelj–Zaversnik bin sort), O(n + m).
// order[i] is the i-th peeled vertex; core[v] is v's core number.
template <class ForEachNeighbor>
void degeneracy_order(int32_t n,
                      std::vector<int32_t> deg,
                      ForEachNeighbor&& for_each_neighbor,
                      std::vector<int32_t>& order,
                      std::vector<int32_t>& core) {
    order.assign(static_cast<std::size_t>(n), 0);
    core.assign(static_cast<std::size_t>(n), 0);
    if (n == 0) return;

    const int32_t max_deg = *std::max_element(deg.begin(), deg.end());
    std::vector<int32_t> start(static_cast<std::size_t>(max_deg) + 2, 0);
    for (int32_t v = 0; v < n; ++v) ++start[static_cast<std::size_t>(deg[static_cast<std::size_t>(v)]) + 1];
    for (std::size_t d = 1; d < start.size(); ++d) start[d] += start[d - 1];

    std::vector<int32_t> pos(static_cast<std::size_t>(n));
    {
        std::vector<int32_t> cursor(start.begin(), start.end() - 1);
        for (int32_t v = 0; v < n; ++v) {
            const auto d = static_cast<std::size_t>(deg[static_cast<std::size_t>(v)]);
            pos[static_cast<std::size_t>(v)] = cursor[d]++;
            order[static_cast<std::size_t>(pos[static_cast<std::size_t>(v)])] = v;
        }
    }

    for (int32_t i = 0; i < n; ++i) {
        const int32_t v = order[static_cast<std::size_t>(i)];
        const int32_t dv = deg[static_cast<std::size_t>(v)];
        core[static_cast<std::size_t>(v)] = dv;
        for_each_neighbor(v, [&](int32_t u) {
            const int32_t du = deg[static_cast<std::size_t>(u)];
            if (du <= dv) return;
            // Move u to the front of its bucket, then shrink that bucket.
            const int32_t pu = pos[static_cast<std::size_t>(u)];
            const int32_t pw = start[static_cast<std::size_t>(du)];
            const int32_t w = order[static_cast<std::size_t>(pw)];
            if (u != w) {
                order[static_cast<std::size_t>(pu)] = w;
                order[static_cast<std::size_t>(pw)] = u;
                pos[static_cast<std::size_t>(u)] = pw;
                pos[static_cast<std::size_t>(w)] = pu;
            }
            ++start[static_cast<std::size_t>(du)];
            deg[static_cast<std::size_t>(u)] = du - 1;
        });
    }
}

// Branch and bound over bitsets. Vertices are expected to be numbered so that
// low ids have high coreness, which makes the greedy coloring tighter.
class BitCliqueSearch {
public:
    BitCliqueSearch(const BitGraph& g,
                    int32_t lower_bound,
                    int32_t max_depth,
                    const std::atomic<int32_t>* shared_size,
                    int32_t shared_offset)
        : g_(g),
          words_(static_cast<std::size_t>(g.words())),
          lb_(lower_bound),
          shared_(shared_size),
          offset_(shared_offset),
          levels_(static_cast<std::size_t>(max_depth) + 2) {
        P_.assign(levels_ * words_, 0);
        U_.assign(words_, 0);
        Q_.assign(words_, 0);
        order_.resize(levels_);
        color_.resize(levels_);
    }

    std::vector<int32_t> run() {
        const int32_t n = g_.size();
        std::uint64_t* P = level(0);
        for (int32_t v = 0; v < n; ++v) P[v >> 6] |= std::uint64_t{1} << (v & 63);
        expand(0);
        return best_;
    }

private:
    std::uint64_t* level(std::size_t d) { return P_.data() + d * words_; }

    int32_t bound() const {
        int32_t b = lb_;
        if (shared_) {
            b = std::max(b, shared_->load(std::memory_order_relaxed) - offset_);
        }
        return b;
    }

    void expand(std::size_t depth) {
        std::uint64_t* P = level(depth);
        const auto c_size = static_cast<int32_t>(C_.size());

        // Greedy coloring of P. Only vertices whose color could still lead to
        // a clique larger than the bound become branching candidates.
        int32_t kmin = bound() - c_size + 1;
        if (kmin < 1) kmin = 1;

        auto& order = order_[depth];
        auto& color = color_[depth];
        order.clear();
        color.clear();

        std::copy(P, P + words_, U_.begin());
        int32_t k = 0;
        bool u_nonempty = std::any_of(U_.begin(), U_.end(), [](std::uint64_t x) { return x != 0; });
        while (u_nonempty) {
            ++k;
            std::copy(U_.begin(), U_.end(), Q_.begin());
            for (std::size_t w = 0; w < words_; ++w) {
                while (Q_[w]) {
                    const int b = ctz64(Q_[w]);
                    const auto v = static_cast<int32_t>(w * 64 + static_cast<std::size_t>(b));
                    const std::uint64_t bit = std::uint64_t{1} << b;
                    U_[w] &= ~bit;
                    Q_[w] &= ~bit;
                    const std::uint64_t* r = g_.row(v);
                    for (std::size_t w2 = w; w2 < words_; ++w2) Q_[w2] &= ~r[w2];
                    if (k >= kmin) {
                        order.push_back(v);
                        color.push_back(k);
                    }
                }
            }
            u_nonempty = std::any_of(U_.begin(), U_.end(), [](std::uint64_t x) { return x != 0; });
        }

        std::uint64_t* NP = level(depth + 1);
        for (std::size_t i = order.size(); i-- > 0;) {
            if (c_size + color[i] <= bound()) return;

            const int32_t v = order[i];
            const std::uint64_t* r = g_.row(v);
            bool any = false;
            for (std::size_t w = 0; w < words_; ++w) {
                NP[w] = P[w] & r[w];
                any |= NP[w] != 0;
            }

            C_.push_back(v);
            if (!any) {
                if (static_cast<int32_t>(C_.size()) > bound()) {
                    best_ = C_;
                    lb_ = static_cast<int32_t>(C_.size());
                }
            } else {
                expand(depth + 1);
            }
            C_.pop_back();
            P[v >> 6] &= ~(std::uint64_t{1} << (v & 63));
        }
    }

    const BitGraph& g_;
    const std::size_t words_;
    int32_t lb_;
    const std::atomic<int32_t>* shared_;
    const int32_t offset_;
    const std::size_t levels_;

    std::vector<std::uint64_t> P_; // candidate set per depth
    std::vector<std::uint64_t> U_; // coloring scratch: uncolored vertices
    std::vector<std::uint64_t> Q_; // coloring scratch: current color class
    std::vector<std::vector<int32_t>> order_;
    std::vector<std::vector<int32_t>> color_;
    std::vector<int32_t> C_;
    std::vector<int32_t> best_;
};

} // namespace

// ------------------------------------------------------------
// BitGraph
// ------------------------------------------------------------

void BitGraph::reset(int32_t n) {
    n_ = n;
    words_ = (n + 63) / 64;
    bits_.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(words_), 0);
}

int32_t BitGraph::degree(int32_t v) const {
    const std::uint64_t* r = row(v);
    int32_t d = 0;
    for (int32_t w = 0; w < words_; ++w) d += popcount64(r[w]);
    return d;
}

// ------------------------------------------------------------
// Solvers
// ------------------------------------------------------------

std::vector<int32_t> max_clique_bitset(const BitGraph& g,
                                       int32_t lower_bound,
                                       const std::atomic<int32_t>* shared_size,
                                       int32_t shared_offset) {
    const int32_t n = g.size();
    if (n == 0) return {};

    auto current_bound = [&]() {
        int32_t b = lower_bound;
        if (shared_size) {
            b = std::max(b, shared_size->load(std::memory_order_relaxed) - shared_offset);
        }
        return b;
    };

    // Degeneracy order: gives an upper bound (max core + 1), lets us drop
    // vertices whose core is too small, and is a good branching order.
    std::vector<int32_t> deg(static_cast<std::size_t>(n));
    for (int32_t v = 0; v < n; ++v) deg[static_cast<std::size_t>(v)] = g.degree(v);

    std::vector<int32_t> order, core;
    degeneracy_order(n, deg,
        [&](int32_t v, auto&& visit) {
            const std::uint64_t* r = g.row(v);
            for (int32_t w = 0; w < g.words(); ++w) {
                std::uint64_t x = r[w];
                while (x) {
                    visit(w * 64 + ctz64(x));
                    x &= x - 1;
                }
            }
        },
        order, core);

    const int32_t max_core = *std::max_element(core.begin(), core.end());
    const int32_t bound = current_bound();
    if (max_core + 1 <= bound) return {};

    // A clique with more than `bound` vertices only uses vertices of core
    // >= bound. Keep those, highest core first.
    std::vector<int32_t> keep;
    keep.reserve(static_cast<std::size_t>(n));
    for (int32_t i = n; i-- > 0;) {
        const int32_t v = order[static_cast<std::size_t>(i)];
        if (core[static_cast<std::size_t>(v)] >= bound) keep.push_back(v);
    }
    if (static_cast<int32_t>(keep.size()) <= bound) return {};

    const auto m = static_cast<int32_t>(keep.size());
    std::vector<int32_t> new_id(static_cast<std::size_t>(n), -1);
    for (int32_t i = 0; i < m; ++i) new_id[static_cast<std::size_t>(keep[static_cast<std::size_t>(i)])] = i;

    BitGraph h(m);
    for (int32_t i = 0; i < m; ++i) {
        const std::uint64_t* r = g.row(keep[static_cast<std::size_t>(i)]);
        for (int32_t w = 0; w < g.words(); ++w) {
            std::uint64_t x = r[w];
            while (x) {
                const int32_t j = new_id[static_cast<std::size_t>(w * 64 + ctz64(x))];
                if (j > i) h.add_edge(i, j);
                x &= x - 1;
            }
        }
    }

    BitCliqueSearch search(h, lower_bound, max_core + 1, shared_size, shared_offset);
    std::vector<int32_t> clique = search.run();
    for (int32_t& v : clique) v = keep[static_cast<std::size_t>(v)];
    return clique;
}

std::vector<int32_t> max_clique_adj(const AdjList& g,
                                    int32_t lower_bound,
                                    const std::atomic<int32_t>* shared_size,
                                    int32_t shared_offset) {
    const auto n = static_cast<int32_t>(g.size());
    if (n == 0) return {};

    if (n <= kDirectBitsetLimit) {
        BitGraph bg(n);
        for (int32_t u = 0; u < n; ++u) {
            for (int32_t v : g[static_cast<std::size_t>(u)]) {
                if (v > u && v < n) bg.add_edge(u, v);
            }
        }
        return max_clique_bitset(bg, lower_bound, shared_size, shared_offset);
    }

    // Large graph: every clique has a unique first vertex u in degeneracy
    // order, and its other vertices are later neighbors of u. There are at
    // most degeneracy of those, so each subproblem is small.
    std::vector<int32_t> deg(static_cast<std::size_t>(n));
    for (int32_t v = 0; v < n; ++v) deg[static_cast<std::size_t>(v)] = static_cast<int32_t>(g[static_cast<std::size_t>(v)].size());

    std::vector<int32_t> order, core;
    degeneracy_order(n, deg,
        [&](int32_t v, auto&& visit) {
            for (int32_t u : g[static_cast<std::size_t>(v)]) visit(u);
        },
        order, core);

    std::vector<int32_t> pos(static_cast<std::size_t>(n));
    for (int32_t i = 0; i < n; ++i) pos[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])] = i;

    std::vector<int32_t> best;
    int32_t lb = lower_bound;
    std::vector<int32_t> local_id(static_cast<std::size_t>(n), -1);
    std::vector<int32_t> later;

    for (int32_t i = n; i-- > 0;) {
        const int32_t u = order[static_cast<std::size_t>(i)];
        int32_t bound = lb;
        if (shared_size) {
            bound = std::max(bound, shared_size->load(std::memory_order_relaxed) - shared_offset);
        }
        if (core[static_cast<std::size_t>(u)] < bound) continue;

        later.clear();
        for (int32_t w : g[static_cast<std::size_t>(u)]) {
            if (pos[static_cast<std::size_t>(w)] > i) later.push_back(w);
        }
        if (static_cast<int32_t>(later.size()) < bound) continue;

        const auto m = static_cast<int32_t>(later.size());
        for (int32_t j = 0; j < m; ++j) local_id[static_cast<std::size_t>(later[static_cast<std::size_t>(j)])] = j;
        BitGraph sub(m);
        for (int32_t j = 0; j < m; ++j) {
            for (int32_t w : g[static_cast<std::size_t>(later[static_cast<std::size_t>(j)])]) {
                const int32_t k = local_id[static_cast<std::size_t>(w)];
                if (k > j) sub.add_edge(j, k);
            }
        }
        for (int32_t w : later) local_id[static_cast<std::size_t>(w)] = -1;

        std::vector<int32_t> found =
            max_clique_bitset(sub, bound - 1, shared_size, shared_offset + 1);
        if (!found.empty()) {
            best.clear();
            best.push_back(u);
            for (int32_t j : found) best.push_back(later[static_cast<std::size_t>(j)]);
            lb = static_cast<int32_t>(best.size());
        }
    }
    return best;
}

} // namespace lazymc
