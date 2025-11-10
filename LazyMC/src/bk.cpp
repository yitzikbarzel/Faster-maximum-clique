// Placeholder BK implementations (no-op)
#include <vector>
#include <cstdint>
namespace lazymc {
struct CliqueResult { std::vector<int32_t> clique; };
CliqueResult bron_kerbosch_basic(const std::vector<std::vector<int32_t>>&){ return {}; }
CliqueResult bron_kerbosch_pivot(const std::vector<std::vector<int32_t>>&){ return {}; }
} // namespace lazymc
