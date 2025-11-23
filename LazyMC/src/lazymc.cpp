#include <vector>
#include <cstdint>
namespace lazymc {
struct MCResult { std::vector<int32_t> clique; int64_t nodes = 0; double ms = 0.0; };
MCResult run_lazymc(const std::vector<std::vector<int32_t>>& /*G*/) {
  // TODO: Wire components (seeding, branching, pruning, BK)
  return {};
}
} // namespace lazymc
