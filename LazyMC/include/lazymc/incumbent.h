#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace lazymc {

extern std::shared_ptr<const std::vector<int32_t>> g_incumbent_clique;
extern std::atomic<int32_t> g_best_size;

inline void reset_incumbent() {
    std::atomic_store_explicit(
        &g_incumbent_clique,
        std::make_shared<const std::vector<int32_t>>(),
        std::memory_order_release);
    g_best_size.store(0, std::memory_order_relaxed);
}

inline void try_update_incumbent(std::vector<int32_t>&& candidate) {
    auto new_ptr =
        std::make_shared<const std::vector<int32_t>>(std::move(candidate));
    const int32_t s = static_cast<int32_t>(new_ptr->size());

    while (true) {
        auto current =
            std::atomic_load_explicit(&g_incumbent_clique,
                                      std::memory_order_acquire);
        const int32_t current_size =
            current ? static_cast<int32_t>(current->size()) : 0;
        if (s <= current_size) return;

        if (std::atomic_compare_exchange_weak_explicit(
                &g_incumbent_clique,
                &current,
                new_ptr,
                std::memory_order_release,
                std::memory_order_acquire)) {
            g_best_size.store(s, std::memory_order_relaxed);
            return;
        }
    }
}

inline void sync_incumbent_size() {
    auto current =
        std::atomic_load_explicit(&g_incumbent_clique,
                                  std::memory_order_acquire);
    const int32_t current_size =
        current ? static_cast<int32_t>(current->size()) : 0;
    g_best_size.store(current_size, std::memory_order_relaxed);
}

} // namespace lazymc
