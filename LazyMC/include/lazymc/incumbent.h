#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace lazymc {

// The best clique found so far (C* in the paper), in G-labels.
//
// One Incumbent is shared by all stages and worker threads of a single
// LazyMC run. size() is a lock-free read meant for pruning; the clique itself
// is only written when it improves, which is rare, so a mutex is enough.
class Incumbent {
public:
    Incumbent() = default;
    Incumbent(const Incumbent&) = delete;
    Incumbent& operator=(const Incumbent&) = delete;

    int32_t size() const noexcept {
        return size_.load(std::memory_order_acquire);
    }

    // Replaces the incumbent if `candidate` is strictly larger.
    // Returns true if the incumbent was replaced.
    bool try_update(std::vector<int32_t> candidate) {
        const auto s = static_cast<int32_t>(candidate.size());
        if (s <= size()) return false;

        std::lock_guard<std::mutex> lock(mtx_);
        if (s <= static_cast<int32_t>(clique_.size())) return false;
        clique_ = std::move(candidate);
        size_.store(s, std::memory_order_release);
        return true;
    }

    std::vector<int32_t> clique() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return clique_;
    }

    const std::atomic<int32_t>& size_ref() const noexcept { return size_; }

private:
    std::atomic<int32_t> size_{0};
    mutable std::mutex mtx_;
    std::vector<int32_t> clique_;
};

} // namespace lazymc
