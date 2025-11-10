// Fast intersection helpers:
//  - If inputs are unsorted: use hash of the smaller set (average O(n)).
//  - If inputs are known sorted: a two-pointer sweep is faster and cache-friendly.

#include <vector>
#include <cstdint>
#include <unordered_set>
#include <algorithm>

namespace lazymc {

static inline int intersect_size_hash(const std::vector<int32_t>& A,
                                      const std::vector<int32_t>& B) {
  // Build hash on the smaller vector to reduce memory & collisions
  const std::vector<int32_t>& small = (A.size() <= B.size()) ? A : B;
  const std::vector<int32_t>& large = (A.size() <= B.size()) ? B : A;

  std::unordered_set<int32_t> H;
  H.reserve(small.size());
  for (int32_t x : small) H.insert(x);

  int cnt = 0;
  for (int32_t y : large) if (H.find(y) != H.end()) ++cnt;
  return cnt;
}

static inline bool intersect_size_gt_hash(const std::vector<int32_t>& A,
                                          const std::vector<int32_t>& B,
                                          int t) {
  // Early-exit once we exceed threshold t
  const std::vector<int32_t>& small = (A.size() <= B.size()) ? A : B;
  const std::vector<int32_t>& large = (A.size() <= B.size()) ? B : A;

  std::unordered_set<int32_t> H;
  H.reserve(small.size());
  for (int32_t x : small) H.insert(x);

  int cnt = 0;
  for (int32_t y : large) {
    if (H.find(y) != H.end() && ++cnt > t) return true;
  }
  return false;
}

static inline int intersect_size_twoptr(const std::vector<int32_t>& A,
                                        const std::vector<int32_t>& B) {
  // Requires BOTH A and B sorted ascending.
  int i = 0, j = 0, cnt = 0;
  while (i < (int)A.size() && j < (int)B.size()) {
    if (A[i] == B[j]) { ++cnt; ++i; ++j; }
    else if (A[i] < B[j]) { ++i; }
    else { ++j; }
  }
  return cnt;
}

static inline bool intersect_size_gt_twoptr(const std::vector<int32_t>& A,
                                            const std::vector<int32_t>& B,
                                            int t) {
  // Requires BOTH A and B sorted ascending.
  int i = 0, j = 0, cnt = 0;
  while (i < (int)A.size() && j < (int)B.size()) {
    if (A[i] == B[j]) {
      if (++cnt > t) return true;
      ++i; ++j;
    } else if (A[i] < B[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  return false;
}

bool intersect_size_gt_bool(const std::vector<int32_t>& A,
                            const std::vector<int32_t>& B,
                            int t) {
  // Default to hash implementation (robust for unsorted inputs).
  // If you GUARANTEE sorted inputs, prefer the two-pointer variant.
  return intersect_size_gt_hash(A, B, t);
}

int intersect_size(const std::vector<int32_t>& A,
                   const std::vector<int32_t>& B) {
  // Default to hash implementation.
  return intersect_size_hash(A, B);
}

} // namespace lazymc
