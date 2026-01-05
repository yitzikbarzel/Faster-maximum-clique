#pragma once

#include <vector>
#include <cstdint>

#include "lazymc/lazygraph.h"
#include "lazymc/profiling.h"

namespace lazymc {

// High-level LazyMC entry point (Algorithm 1).
// Returns a clique in G-labels.
std::vector<int32_t> LazyMC_run(const AdjList& G);

// Profiled version of LazyMC_run.
// Returns both the clique and detailed performance statistics for each stage.
ProfilingResult LazyMC_run_profiled(const AdjList& G);

} // namespace lazymc
