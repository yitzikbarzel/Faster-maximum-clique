#include "lazymc/k_vertex_cover.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using lazymc::AdjList;

constexpr long long VC_STEP_LIMIT = 20'000'000;

// Validate that all neighbors in the adjacency lists are within [0, n).
bool validate_graph_indices(const AdjList& G) {
    const auto n = static_cast<int32_t>(G.size());
    for (const auto& nbrs : G) {
        for (int32_t u : nbrs) {
            if (u < 0 || u >= n) {
                return false;
            }
        }
    }
    return true;
}

// Branch and bound for vertex cover.
//
// Looks for covers of size <= limit_. Every time one is found it is recorded
// and limit_ drops to its size - 1, so the search ends with a minimum cover
// (unless stop_at_first_ is set, which answers the plain k-VC question).
//
// State changes are recorded on a trail and undone on backtrack, so a search
// node costs O(n + m) rather than a copy of the whole state.
class VCSearch {
public:
    VCSearch(const AdjList& G, int32_t limit, bool stop_at_first, long long step_limit)
        : G_(G),
          n_(static_cast<int32_t>(G.size())),
          active_(static_cast<std::size_t>(n_), 1),
          deg_(static_cast<std::size_t>(n_), 0),
          matched_(static_cast<std::size_t>(n_), 0),
          limit_(limit),
          stop_at_first_(stop_at_first),
          step_limit_(step_limit) {
        for (int32_t v = 0; v < n_; ++v) {
            deg_[static_cast<std::size_t>(v)] = static_cast<int32_t>(G_[static_cast<std::size_t>(v)].size());
            edges2_ += deg_[static_cast<std::size_t>(v)];
            work_.push_back(v);
        }
    }

    void run() { search(); }

    bool found() const { return found_; }
    bool aborted() const { return aborted_; }
    std::vector<int32_t>& best() { return best_; }

private:
    int32_t budget() const { return limit_ - static_cast<int32_t>(cover_.size()); }

    void remove(int32_t v, bool into_cover) {
        active_[static_cast<std::size_t>(v)] = 0;
        if (into_cover) cover_.push_back(v);
        trail_.push_back(into_cover ? ~v : v);
        for (int32_t u : G_[static_cast<std::size_t>(v)]) {
            if (active_[static_cast<std::size_t>(u)]) {
                --deg_[static_cast<std::size_t>(u)];
                edges2_ -= 2;
                work_.push_back(u);
            }
        }
    }

    void undo_to(std::size_t mark) {
        while (trail_.size() > mark) {
            const int32_t e = trail_.back();
            trail_.pop_back();
            const bool in_cover = e < 0;
            const int32_t v = in_cover ? ~e : e;
            if (in_cover) cover_.pop_back();
            for (int32_t u : G_[static_cast<std::size_t>(v)]) {
                if (active_[static_cast<std::size_t>(u)]) {
                    ++deg_[static_cast<std::size_t>(u)];
                    edges2_ += 2;
                }
            }
            active_[static_cast<std::size_t>(v)] = 1;
        }
    }

    // Reductions that never make the optimum worse:
    //  - deg 0: drop the vertex.
    //  - deg 1: its neighbor can be taken into the cover.
    //  - deg > remaining budget: the vertex must be in any cover within budget.
    // Returns false if the budget is exceeded.
    bool kernelize() {
        while (true) {
            while (!work_.empty()) {
                const int32_t v = work_.back();
                work_.pop_back();
                if (!active_[static_cast<std::size_t>(v)]) continue;
                if (budget() < 0) return false;

                const int32_t d = deg_[static_cast<std::size_t>(v)];
                if (d == 0) {
                    remove(v, false);
                } else if (d == 1) {
                    for (int32_t u : G_[static_cast<std::size_t>(v)]) {
                        if (active_[static_cast<std::size_t>(u)]) {
                            remove(u, true);
                            break;
                        }
                    }
                } else if (d > budget()) {
                    remove(v, true);
                }
            }
            if (budget() < 0) return false;

            // The budget shrank, so the high-degree rule may now apply to
            // vertices that are not on the worklist.
            bool changed = false;
            for (int32_t v = 0; v < n_; ++v) {
                if (active_[static_cast<std::size_t>(v)] &&
                    deg_[static_cast<std::size_t>(v)] > budget()) {
                    remove(v, true);
                    changed = true;
                    if (budget() < 0) return false;
                }
            }
            if (!changed) return true;
        }
    }

    // Size of a greedy maximal matching: a lower bound on any cover.
    int32_t matching_lower_bound() {
        ++stamp_;
        if (stamp_ == 0) {
            std::fill(matched_.begin(), matched_.end(), 0);
            stamp_ = 1;
        }
        int32_t size = 0;
        for (int32_t v = 0; v < n_; ++v) {
            if (!active_[static_cast<std::size_t>(v)] || matched_[static_cast<std::size_t>(v)] == stamp_) continue;
            for (int32_t u : G_[static_cast<std::size_t>(v)]) {
                if (active_[static_cast<std::size_t>(u)] && matched_[static_cast<std::size_t>(u)] != stamp_) {
                    matched_[static_cast<std::size_t>(v)] = stamp_;
                    matched_[static_cast<std::size_t>(u)] = stamp_;
                    ++size;
                    break;
                }
            }
        }
        return size;
    }

    void search() {
        if (done_ || aborted_) return;
        if (++steps_ > step_limit_) {
            aborted_ = true;
            return;
        }

        const std::size_t mark = trail_.size();
        const bool ok = kernelize();
        work_.clear();
        if (!ok) {
            undo_to(mark);
            return;
        }

        if (edges2_ == 0) {
            best_ = cover_;
            found_ = true;
            limit_ = static_cast<int32_t>(cover_.size()) - 1;
            if (stop_at_first_) done_ = true;
            undo_to(mark);
            return;
        }

        int32_t v = -1;
        int32_t max_deg = 0;
        for (int32_t u = 0; u < n_; ++u) {
            if (active_[static_cast<std::size_t>(u)] && deg_[static_cast<std::size_t>(u)] > max_deg) {
                max_deg = deg_[static_cast<std::size_t>(u)];
                v = u;
            }
        }

        // Each cover vertex covers at most max_deg edges.
        const long long edges = edges2_ / 2;
        if (static_cast<long long>(max_deg) * budget() < edges ||
            static_cast<int32_t>(cover_.size()) + matching_lower_bound() > limit_) {
            undo_to(mark);
            return;
        }

        std::vector<int32_t> nbrs;
        nbrs.reserve(static_cast<std::size_t>(max_deg));
        for (int32_t u : G_[static_cast<std::size_t>(v)]) {
            if (active_[static_cast<std::size_t>(u)]) nbrs.push_back(u);
        }

        // Branch A: v is in the cover.
        {
            const std::size_t m2 = trail_.size();
            remove(v, true);
            search();
            work_.clear();
            undo_to(m2);
        }

        // Branch B: v is not in the cover, so all its neighbors are.
        if (!done_ && !aborted_ &&
            static_cast<int32_t>(cover_.size() + nbrs.size()) <= limit_) {
            const std::size_t m2 = trail_.size();
            for (int32_t u : nbrs) remove(u, true);
            search();
            work_.clear();
            undo_to(m2);
        }

        undo_to(mark);
    }

    const AdjList& G_;
    const int32_t n_;
    std::vector<std::uint8_t> active_;
    std::vector<int32_t> deg_;       // degree among active vertices
    std::vector<uint32_t> matched_;  // matching scratch, valid when == stamp_
    uint32_t stamp_ = 0;
    long long edges2_ = 0;           // twice the number of active edges

    std::vector<int32_t> cover_;
    std::vector<int32_t> trail_;     // v (dropped) or ~v (taken into cover)
    std::vector<int32_t> work_;

    int32_t limit_;
    const bool stop_at_first_;
    const long long step_limit_;
    long long steps_ = 0;
    bool done_ = false;
    bool aborted_ = false;
    bool found_ = false;
    std::vector<int32_t> best_;
};

} // anonymous namespace

namespace lazymc {

k_vertex_cover_res k_vertex_cover_adj(const AdjList& G, int32_t k) {
    k_vertex_cover_res result;
    if (k < 0) return result;

    if (!validate_graph_indices(G)) {
        result.aborted = true; // Invalid input detected
        return result;
    }

    VCSearch search(G, k, /*stop_at_first=*/true, VC_STEP_LIMIT);
    search.run();

    result.aborted = search.aborted() && !search.found();
    result.found = search.found();
    if (result.found) result.cover = std::move(search.best());
    return result;
}

min_vertex_cover_res min_vertex_cover_adj(const AdjList& G,
                                          int32_t k_max,
                                          long long step_limit) {
    min_vertex_cover_res result;
    if (k_max < 0) return result;

    if (!validate_graph_indices(G)) {
        result.aborted = true;
        return result;
    }

    VCSearch search(G, k_max, /*stop_at_first=*/false, step_limit);
    search.run();

    result.aborted = search.aborted();
    result.found = search.found();
    result.optimal = result.found && !result.aborted;
    if (result.found) result.cover = std::move(search.best());
    return result;
}

} // namespace lazymc
