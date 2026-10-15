#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h"
#include "lazymc/profiling.h"

namespace lazymc {

// High-level LazyMC entry point (Algorithm 1).
// Returns a maximum clique of G, in G's vertex ids.
//
// G is an undirected graph: G[u] lists the neighbors of u, and every edge
// should appear in both endpoints' lists. Lists that are unsorted or contain
// duplicates, self-loops or out-of-range ids are accepted; a cleaned copy is
// made in that case (an edge listed only on one side is then also treated as
// undirected). Sorted, clean input is used as is, without copying.
std::vector<int32_t> LazyMC_run(const AdjList& G);

// Profiled version of LazyMC_run.
// Returns both the clique and detailed performance statistics for each stage.
ProfilingResult LazyMC_run_profiled(const AdjList& G);

} // namespace lazymc
