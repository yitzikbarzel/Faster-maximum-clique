#include <vector>
#include <cstdint>
namespace lazymc {
// Minimal graph placeholder; no header dependency to keep names unchanged
using Vid = int32_t;
class LazyGraph {
public:
  explicit LazyGraph(std::vector<std::vector<Vid>> adj = {}) : adj_(std::move(adj)) {}
  int n() const { return static_cast<int>(adj_.size()); }
  const std::vector<Vid>& neighbors(Vid v) const { return adj_[v]; }
private:
  std::vector<std::vector<Vid>> adj_;
};
} // namespace lazymc
