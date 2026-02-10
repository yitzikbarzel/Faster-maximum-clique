#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <thread>

namespace lazymc {

// Statistics collected for a single algorithm stage
struct StageStats {
    std::string name;
    double duration_ms{0.0};
    int32_t clique_size_before{0};
    int32_t clique_size_after{0};
    unsigned threads_used{1};
    
    // Optional: additional metrics
    int64_t candidates_processed{0};
    int64_t nodes_pruned{0};
};

// Complete profiling result for a LazyMC run
struct ProfilingResult {
    std::vector<StageStats> stages;
    double total_time_ms{0.0};
    int32_t final_clique_size{0};
    std::vector<int32_t> final_clique;
    
    // Print a summary table to stdout
    void print_summary() const {
        std::cout << "\n";
        std::cout << "========== LazyMC Profiling Summary ==========\n";
        std::cout << std::left << std::setw(30) << "Stage"
                  << std::right << std::setw(12) << "Time (ms)"
                  << std::setw(10) << "Threads"
                  << std::setw(12) << "Clique In"
                  << std::setw(12) << "Clique Out"
                  << "\n";
        std::cout << std::string(76, '-') << "\n";
        
        for (const auto& s : stages) {
            std::cout << std::left << std::setw(30) << s.name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(2) << s.duration_ms
                      << std::setw(10) << s.threads_used
                      << std::setw(12) << s.clique_size_before
                      << std::setw(12) << s.clique_size_after
                      << "\n";
        }
        
        std::cout << std::string(76, '-') << "\n";
        std::cout << std::left << std::setw(30) << "TOTAL"
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << total_time_ms
                  << std::setw(10) << "-"
                  << std::setw(12) << "-"
                  << std::setw(12) << final_clique_size
                  << "\n";
        std::cout << "==============================================\n\n";
    }
};

// RAII timer for measuring stage duration
class ScopedTimer {
public:
    explicit ScopedTimer(double& out_ms)
        : out_ms_(out_ms)
        , start_(std::chrono::high_resolution_clock::now())
    {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        out_ms_ = std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
    // Non-copyable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    
private:
    double& out_ms_;
    std::chrono::high_resolution_clock::time_point start_;
};

// Utility to compute thread count used by parallel algorithms
inline unsigned get_parallel_thread_count(std::size_t work_items) {
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    return std::min(hw, static_cast<unsigned>(work_items));
}

} // namespace lazymc
