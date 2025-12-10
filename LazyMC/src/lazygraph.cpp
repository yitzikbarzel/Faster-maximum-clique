#include "lazymc/Lazygraph.h"

namespace lazymc {

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------

LazyGraph::LazyGraph(const AdjList& G,
                     const std::vector<int32_t>& relabel_to_original,
                     const std::vector<int32_t>& original_to_relabel,
                     const std::vector<int32_t>& core)
    : G_(&G),
      relabel_to_original_(relabel_to_original),
      original_to_relabel_(original_to_relabel),
      core_(core)
{
    std::size_t n = relabel_to_original_.size();

    // Initialize flags: all zero → no neighborhoods built yet
    flags_.assign(n, static_cast<std::uint8_t>(0));

    // Allocate per-vertex containers for hashed and sorted neighborhoods
    hashed_.resize(n);
    sorted_.resize(n);

    // NOTE: No locks_ and no atomics in this single-threaded version.
}

// ------------------------------------------------------------
// Basic information
// ------------------------------------------------------------

std::size_t LazyGraph::num_vertices() const noexcept {
    return relabel_to_original_.size();
}

const AdjList& LazyGraph::original_graph() const noexcept {
    return *G_;
}

const std::vector<int32_t>& LazyGraph::relabel_to_original() const noexcept {
    return relabel_to_original_;
}

const std::vector<int32_t>& LazyGraph::original_to_relabel() const noexcept {
    return original_to_relabel_;
}

int32_t LazyGraph::to_original(int32_t v_H) const {
    return relabel_to_original_[static_cast<std::size_t>(v_H)];
}

int32_t LazyGraph::to_relabelled(int32_t v_G) const {
    return original_to_relabel_[static_cast<std::size_t>(v_G)];
}

const std::vector<int32_t>& LazyGraph::core_values() const noexcept {
    return core_;
}



// ------------------------------------------------------------
// Neighborhood access (placeholder for now)
// ------------------------------------------------------------

LazyGraph::NeighborHash& LazyGraph::hashed_neighbors(int32_t v) {
    // Algorithm 2 logic (lazy build + filtering) will be added later.
    return hashed_[static_cast<std::size_t>(v)];
}

const LazyGraph::NeighborArray& LazyGraph::sorted_neighbors(int32_t v) {
    // Algorithm 2 logic (lazy build + sorting) will be added later.
    return sorted_[static_cast<std::size_t>(v)];
}

// Builders – empty for now; will implement Algorithm 2 here.
void LazyGraph::build_hashed(int32_t /*v*/) {}
void LazyGraph::build_sorted(int32_t /*v*/) {}

} // namespace lazymc
