#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace lazymc {

extern std::vector<int32_t> g_incumbent_clique;
extern std::atomic<int32_t> g_best_size;

inline void reset_incumbent() {
    g_incumbent_clique.clear();
    g_best_size.store(0, std::memory_order_relaxed);
}

inline void try_update_incumbent(std::vector<int32_t>&& candidate) {
    const int32_t s = static_cast<int32_t>(candidate.size());
    if (s <= g_best_size.load(std::memory_order_relaxed)) return;

    g_incumbent_clique = std::move(candidate);
    g_best_size.store(static_cast<int32_t>(g_incumbent_clique.size()),
                      std::memory_order_relaxed);
}

inline void sync_incumbent_size() {
    g_best_size.store(static_cast<int32_t>(g_incumbent_clique.size()),
                      std::memory_order_relaxed);
}

} // namespace lazymc
