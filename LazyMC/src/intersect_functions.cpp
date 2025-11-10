#include <vector>
#include <cstdint>
namespace lazymc {
// Naive O(|A|*|B|) intersection threshold check
bool intersect_size_gt_bool(const std::vector<int32_t>& A,
                            const std::vector<int32_t>& B,
                            int t) {
  int cnt = 0;
  for (int32_t x : A)
    for (int32_t y : B)
      if (x == y && ++cnt > t) return true;
  return false;
}
// Naive O(|A|*|B|) intersection size
int intersect_size(const std::vector<int32_t>& A,
                   const std::vector<int32_t>& B) {
  int cnt = 0;
  for (int32_t x : A)
    for (int32_t y : B)
      if (x == y) ++cnt;
  return cnt;
}
} // namespace lazymc
