#include "lazymc/lazygraph.h"
#include "lazymc/incumbent.h"

#include <algorithm>
#include <cstddef>

namespace lazymc {

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------

LazyGraph::LazyGraph(const AdjList& G,
                     const std::vector<int32_t>& relabel_to_original,
                     const std::vector<int32_t>& original_to_relabel,
                     const std::vector<int32_t>& core,
                     const Incumbent* incumbent)
    : G_(&G),
      incumbent_(incumbent),
      relabel_to_original_(relabel_to_original),
      original_to_relabel_(original_to_relabel),
      core_(core)
{
    const std::size_t n = relabel_to_original_.size();

    hashed_.resize(n);
    sorted_.resize(n);

    // Allocate once_flag arrays (default constructed)
    hashed_once_ = std::make_unique<std::once_flag[]>(n);
    sorted_once_ = std::make_unique<std::once_flag[]>(n);
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
// Neighborhood access (thread-safe lazy)
// ------------------------------------------------------------

LazyGraph::NeighborHash& LazyGraph::hashed_neighbors(int32_t v) {
    const std::size_t idx = static_cast<std::size_t>(v);

    std::call_once(hashed_once_[idx], [this, v]() {
        build_hashed(v);
    });

    return *hashed_[idx];
}

const LazyGraph::NeighborArray& LazyGraph::sorted_neighbors(int32_t v) {
    const std::size_t idx = static_cast<std::size_t>(v);

    std::call_once(sorted_once_[idx], [this, v]() {
        build_sorted(v);
    });

    return sorted_[idx];
}

// ------------------------------------------------------------
// Builders
// ------------------------------------------------------------

// The sorted array is the primary representation and is built directly from
// G; the hash set is derived from it on demand.
void LazyGraph::build_sorted(int32_t v) {
    const std::size_t idx = static_cast<std::size_t>(v);

    const int32_t v_G = relabel_to_original_[idx];
    const auto& neighbors_G = (*G_)[static_cast<std::size_t>(v_G)];
    const int32_t incumbent = incumbent_ ? incumbent_->size() : 0;

    NeighborArray& out = sorted_[idx];
    out.clear();
    out.reserve(neighbors_G.size());

    for (int32_t u_G : neighbors_G) {
        const std::size_t u_idx = static_cast<std::size_t>(u_G);
        if (u_idx >= original_to_relabel_.size()) {
            continue;
        }

        const int32_t u_H = original_to_relabel_[u_idx];
        if (u_H >= 0 && u_H != v &&
            core_[static_cast<std::size_t>(u_H)] >= incumbent) {
            out.push_back(u_H);
        }
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    out.shrink_to_fit();
}

void LazyGraph::build_hashed(int32_t v) {
    const auto& sorted = sorted_neighbors(v);
    hashed_[static_cast<std::size_t>(v)] =
        std::make_unique<NeighborHash>(sorted.begin(), sorted.end());
}

} // namespace lazymc
