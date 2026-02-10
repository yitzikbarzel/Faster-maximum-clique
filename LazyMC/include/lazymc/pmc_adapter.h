#pragma once

#include <cstdint>
#include <vector>

namespace lazymc {

struct PmcResult {
    std::vector<int32_t> clique;
};

PmcResult pmc_max_clique(const std::vector<std::vector<int32_t>>& G, int index_offset);

} // namespace lazymc
