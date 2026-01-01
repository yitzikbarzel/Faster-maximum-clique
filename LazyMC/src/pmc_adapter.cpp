#include "lazymc/pmc_adapter.h"

#include <stdexcept>
#include <vector>

// PMC C API
extern "C" {
int max_clique(long long nedges, int* ei, int* ej, int index_offset,
               int outsize, int* clique);
}

namespace lazymc {

PmcResult pmc_max_clique(const std::vector<std::vector<int32_t>>& G, int index_offset) {
    const int32_t n = static_cast<int32_t>(G.size());
    if (n == 0) return {};

    // Build a directed edge list (one entry per adjacency).
// PMC expects symmetric edges for undirected graphs: if (u,v) exists then (v,u) exists.
std::vector<int> ei;
std::vector<int> ej;

// Optional: reserve to reduce reallocations (2*m). If you don't know m here, keep 1024.
ei.reserve(1024);
ej.reserve(1024);

for (int32_t u = 0; u < n; ++u) {
    for (int32_t v : G[static_cast<std::size_t>(u)]) {
        if (v == u) continue;
        ei.push_back(static_cast<int>(u + index_offset));
        ej.push_back(static_cast<int>(v + index_offset));
    }
}


    std::vector<int> out(static_cast<std::size_t>(n), 0);

    const long long m = static_cast<long long>(ei.size());
    const int C = max_clique(m, ei.data(), ej.data(), index_offset, n, out.data());
    if (C < 0 || C > n) {
        throw std::runtime_error("PMC returned invalid clique size");
    }

    PmcResult res;
    res.clique.reserve(static_cast<std::size_t>(C));
    for (int i = 0; i < C; ++i) {
        // Convert back to 0-based ids for your code.
        res.clique.push_back(static_cast<int32_t>(out[i] - index_offset));
    }
    return res;
}

} // namespace lazymc
