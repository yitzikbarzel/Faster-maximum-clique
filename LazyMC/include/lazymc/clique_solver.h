#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "lazymc/lazygraph.h" // for AdjList

namespace lazymc {

// Undirected graph on vertices 0..n-1 stored as adjacency bitsets.
// Meant for the small local subgraphs that NeighborSearch hands to the exact
// solvers.
class BitGraph {
public:
    explicit BitGraph(int32_t n = 0) { reset(n); }

    void reset(int32_t n);

    int32_t size() const noexcept { return n_; }
    int32_t words() const noexcept { return words_; }

    void add_edge(int32_t u, int32_t v) {
        set_bit(u, v);
        set_bit(v, u);
    }
    bool has_edge(int32_t u, int32_t v) const {
        return (row(u)[v >> 6] >> (v & 63)) & 1u;
    }
    const std::uint64_t* row(int32_t v) const {
        return bits_.data() + static_cast<std::size_t>(v) * static_cast<std::size_t>(words_);
    }
    int32_t degree(int32_t v) const;

private:
    void set_bit(int32_t u, int32_t v) {
        bits_[static_cast<std::size_t>(u) * static_cast<std::size_t>(words_) +
              static_cast<std::size_t>(v >> 6)] |= std::uint64_t{1} << (v & 63);
    }

    int32_t n_ = 0;
    int32_t words_ = 0;
    std::vector<std::uint64_t> bits_;
};

// Exact maximum clique of `g`, searching only for cliques with more than
// `lower_bound` vertices. Branch and bound with greedy-coloring bounds over
// bitsets (MCQ/BBMC style).
//
// If `shared_size` is given, (*shared_size - shared_offset) is used as an
// extra lower bound that may grow while the search runs; this lets parallel
// searches prune against each other's results.
//
// Returns the clique (vertex ids of `g`), or an empty vector if no clique
// larger than the bound exists.
std::vector<int32_t> max_clique_bitset(const BitGraph& g,
                                       int32_t lower_bound,
                                       const std::atomic<int32_t>* shared_size = nullptr,
                                       int32_t shared_offset = 0);

// Same contract as max_clique_bitset, for a graph given as sorted adjacency
// lists. Graphs up to 32768 vertices are solved directly with bitsets; larger
// ones are split along a degeneracy order so that every bitset subproblem has
// at most degeneracy+1 vertices.
std::vector<int32_t> max_clique_adj(const AdjList& g,
                                    int32_t lower_bound,
                                    const std::atomic<int32_t>* shared_size = nullptr,
                                    int32_t shared_offset = 0);

} // namespace lazymc
