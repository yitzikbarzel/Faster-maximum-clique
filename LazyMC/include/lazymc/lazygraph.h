#pragma once

#include <cstdint>
#include <mutex>        // std::once_flag, std::call_once
#include <memory>       // std::unique_ptr
#include <unordered_set>
#include <vector>

namespace lazymc {

class Incumbent;

// Original adjacency list: vertices labeled 0..n-1.
using AdjList = std::vector<std::vector<int32_t>>;

class LazyGraph {
public:
    using NeighborHash  = std::unordered_set<int32_t>;
    using NeighborArray = std::vector<int32_t>;

    // If `incumbent` is given, neighborhoods are filtered when they are
    // first built: neighbors u with core[u] < |C*| are dropped, since they
    // cannot be part of a clique larger than C*. Without it nothing is
    // filtered.
    LazyGraph(const AdjList& G,
              const std::vector<int32_t>& relabel_to_original,
              const std::vector<int32_t>& original_to_relabel,
              const std::vector<int32_t>& core,
              const Incumbent* incumbent = nullptr);

    // once_flag is non-copyable/non-movable → disallow copy/move
    LazyGraph(const LazyGraph&) = delete;
    LazyGraph& operator=(const LazyGraph&) = delete;
    LazyGraph(LazyGraph&&) = delete;
    LazyGraph& operator=(LazyGraph&&) = delete;

    // ---------- basic information ----------
    std::size_t num_vertices() const noexcept;
    const AdjList& original_graph() const noexcept;

    const std::vector<int32_t>& relabel_to_original() const noexcept;
    const std::vector<int32_t>& original_to_relabel() const noexcept;

    int32_t to_original(int32_t v_H) const;
    int32_t to_relabelled(int32_t v_G) const;

    const std::vector<int32_t>& core_values() const noexcept;

    // ---------- lazy neighborhood access ----------
    NeighborHash& hashed_neighbors(int32_t v);
    const NeighborArray& sorted_neighbors(int32_t v);

private:
    void build_hashed(int32_t v);
    void build_sorted(int32_t v);

private:
    const AdjList* G_ = nullptr;
    const Incumbent* incumbent_ = nullptr;

    std::vector<int32_t> relabel_to_original_;
    std::vector<int32_t> original_to_relabel_;
    std::vector<int32_t> core_;

    // Hash sets are allocated on first use only: most vertices never need one.
    std::vector<std::unique_ptr<NeighborHash>> hashed_;
    std::vector<NeighborArray> sorted_;

    // Thread-safe lazy init flags (array, not vector)
    std::unique_ptr<std::once_flag[]> hashed_once_;
    std::unique_ptr<std::once_flag[]> sorted_once_;
};

} // namespace lazymc
