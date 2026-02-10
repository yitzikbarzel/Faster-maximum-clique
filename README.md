`C++17` | `CMake 3.20+` | `Windows / Linux / WSL`

# LazyMC — Faster Maximum Clique by Work-Avoidance

A high-performance C++17 implementation of the **LazyMC** algorithm for solving the **Maximum Clique Problem (MCP)** on large sparse graphs, based on the paper:

> **"Less is More: Faster Maximum Clique Search by Work-Avoidance"**
> H. Vandierendonck, _IEEE International Parallel and Distributed Processing Symposium (IPDPS), 2025_
> [DOI: 10.1109/IPDPS64566.2025.00025](https://doi.org/10.1109/IPDPS64566.2025.00025)

---

## Overview

The Maximum Clique problem asks: _given an undirected graph, find the largest fully-connected subgraph._ This is NP-hard in the worst case, but LazyMC exploits structural properties of real-world graphs to solve instances with **millions of vertices and tens of millions of edges** in seconds.

LazyMC introduces four key optimization techniques over traditional branch-and-bound MC algorithms:

| Technique                    | Description                                                                                               |
| ---------------------------- | --------------------------------------------------------------------------------------------------------- |
| **Lazy Graph Construction**  | Neighborhoods are built on-demand, filtered by the incumbent clique size at construction time             |
| **Multi-Stage Filtering**    | Three-pass filtering eliminates candidate vertices _before_ search, often ruling out entire neighborhoods |
| **Early-Exit Intersections** | Set intersections abort early when the result cannot meet the required size threshold                     |
| **Algorithmic Choice**       | Dense subgraphs are solved via k-Vertex Cover on the complement graph instead of direct MC search         |

---

## Performance

Benchmarked against [PMC](https://github.com/ryanrossi/pmc) (Parallel Maximum Clique):

| Dataset     | Vertices  | Edges      | Clique | LazyMC      | PMC     |
| ----------- | --------- | ---------- | ------ | ----------- | ------- |
| RoadNet-CA  | 1,971,300 | 2,766,607  | 4      | **0.38 s**  | 1.91 s  |
| Bio-HS-CX   | 4,413     | 108,818    | 86     | **0.05 s**  | 0.08 s  |
| YouTube     | 1,157,827 | 2,987,624  | 17     | **2.30 s**  | 3.29 s  |
| LiveJournal | 4,847,571 | 42,851,237 | 321    | **20.88 s** | 37.99 s |

---

## Algorithm Pipeline

LazyMC follows a structured multi-stage approach (Algorithm 1 from the paper):

```
Input Graph G
     |
     v
[1] Degree-Based Heuristic Search -----> Initial incumbent C*
     |
     v
[2] K-Core Decomposition (d >= |C*|) --> Coreness values
     |
     v
[3] Vertex Reordering (by coreness) ---> Sort order O
     |
     v
[4] Build LazyGraph H(G, O, core) -----> Lazy filtered hashed representation
     |
     v
[5] Coreness-Based Heuristic Search ---> Improved C*
     |
     v
[6] Systematic Search -----------------> Exact maximum clique
     |
     v
Output: Maximum clique
```

Each stage progressively narrows the search space. In many real-world graphs, over 99.9% of neighborhoods are eliminated by filtering alone, and the systematic search only processes a handful of candidate subgraphs.

---

## Project Structure

```
LazyMC/
├── include/lazymc/          # Public headers
│   ├── lazymc.h             # Main API: LazyMC_run(), LazyMC_run_profiled()
│   ├── lazygraph.h          # Lazy filtered hashed relabelled graph
│   ├── incumbent.h          # Lock-free atomic incumbent management
│   ├── bk.h                 # Bron-Kerbosch (MC solver for subproblems)
│   ├── k_vertex_cover.h     # k-VC solver for dense subgraphs
│   ├── kcore.h              # K-core decomposition
│   ├── intersect_functions.h # Early-exit set intersections
│   ├── neighbor_search.h    # Per-vertex neighborhood search
│   ├── systematic_search.h  # Multi-threaded systematic search
│   ├── degree_heuristic.h   # Degree-based heuristic (Algorithm 5)
│   ├── coreness_heuristic.h # Coreness-based heuristic (Algorithm 6)
│   ├── profiling.h          # Profiling infrastructure
│   └── ...
├── src/                     # Implementation (~3,300 lines)
│   ├── lazymc.cpp           # Algorithm 1 orchestration
│   ├── bench_cli.cpp        # Benchmarking CLI (LazyMC vs PMC)
│   └── ...
├── tests/                   # GoogleTest suite (12+ test files)
│   ├── data/                # Real-world benchmark graphs
│   └── test_*.cpp
└── CMakeLists.txt           # Build configuration
```

---

## Building

### Prerequisites

- **C++17** compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- **CMake** 3.20 or later
- **Git** (for fetching dependencies)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/KamaTechOrg/Faster-maximum-clique.git
cd Faster-maximum-clique/LazyMC

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests
cd build && ctest --output-on-failure
```

Dependencies ([PMC](https://github.com/ryanrossi/pmc) and [GoogleTest](https://github.com/google/googletest)) are fetched automatically via CMake `FetchContent`.

---

## Usage

### Benchmarking CLI

```bash
# Compare LazyMC vs PMC on a graph file
./build/bench_cli path/to/graph.txt

# With options
./build/bench_cli path/to/graph.txt --solver all --one-based
```

### Supported Input Formats

**Edge list with header** (default):

```
<num_vertices> <num_edges>
<u> <v>
<u> <v>
...
```

**DIMACS clique format**:

```
p edge <num_vertices> <num_edges>
e <u> <v>
e <u> <v>
...
```

Lines starting with `#` or `c` are treated as comments. Self-loops are automatically removed and duplicate edges are deduplicated.

### Library API

```cpp
#include "lazymc/lazymc.h"

using AdjList = std::vector<std::vector<int32_t>>;

// Build your adjacency list (0-based, undirected)
AdjList G(n);
G[u].push_back(v);
G[v].push_back(u);

// Find maximum clique
std::vector<int32_t> clique = lazymc::LazyMC_run(G);

// Or with profiling
lazymc::ProfilingResult result = lazymc::LazyMC_run_profiled(G);
result.print_summary();
```

---

## Key Components

### LazyGraph (Algorithm 2)

Thread-safe, lazily-constructed graph representation with:

- On-demand hashed neighborhood construction via double-checked locking
- Filtered by incumbent clique size at construction time
- Vertex relabeling (original <-> coreness-ordered)

### Early-Exit Set Intersections (Algorithms 3-4)

- **`intersect_gt(A, B, C, theta)`** — computes `A ∩ B` into `C` only if `|A ∩ B| > theta`
- **`intersect_size_gt_bool(A, B, theta)`** — returns `|A ∩ B| > theta` with dual early-exit (both true and false)

### Algorithmic Choice (Section IV-E)

Subgraphs with density above threshold phi are solved using **k-Vertex Cover** on the complement graph. For sparse subgraphs, a **Bron-Kerbosch** based MC solver with Tomita pivoting and graph coloring pruning is used.

### Thread Safety

- Lock-free incumbent updates via `std::atomic` CAS operations
- `std::shared_ptr` with atomic load/store for clique snapshots
- `std::once_flag` for lazy neighborhood initialization

---

## Testing

The test suite validates each algorithm component independently:

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./tests/lazymc_tests --gtest_filter="BronKerbosch*"
```

Test coverage includes:

- Bron-Kerbosch correctness on known graphs
- K-core decomposition verification
- Early-exit intersection correctness
- LazyGraph lazy initialization
- End-to-end algorithm correctness on random graphs
- K-Vertex Cover solver

---

## Datasets

Benchmark datasets are sourced from the [Stanford Large Network Dataset Collection (SNAP)](https://snap.stanford.edu/data/) and the [Network Data Repository](https://networkrepository.com/):

| Dataset          | Source             | Vertices | Edges |
| ---------------- | ------------------ | -------- | ----- |
| roadNet-CA       | SNAP               | 1.97M    | 2.77M |
| com-youtube      | SNAP               | 1.16M    | 2.99M |
| soc-LiveJournal1 | SNAP               | 4.85M    | 42.9M |
| sx-stackoverflow | SNAP               | 2.58M    | 28.2M |
| bio-HS-CX        | Network Repository | 4.4K     | 109K  |

---

## Known Limitations / TODOs

- **`top_k` parameter is hardcoded**: The `top_k` value used in the degree-based heuristic search (currently set to 120 in `src/lazymc.cpp`) is an arbitrary constant. Ideally, it should be determined dynamically based on the graph size and density.

---

## References

```bibtex
@inproceedings{vandierendonck2025lazymc,
  author    = {Vandierendonck, Hans},
  title     = {Less is More: Faster Maximum Clique Search by Work-Avoidance},
  booktitle = {IEEE International Parallel and Distributed Processing
               Symposium (IPDPS)},
  pages     = {187--198},
  year      = {2025},
  doi       = {10.1109/IPDPS64566.2025.00025}
}
```

Key related works:

- **PMC** — R. Rossi et al., _Fast Maximum Clique Algorithms for Large Graphs_, WWW 2014
- **MC-BRB** — L. Chang, _Efficient Maximum Clique Computation over Large Sparse Graphs_, KDD 2019
- **dOmega** — J. Walteros & A. Buchanan, _Why is Maximum Clique Often Easy in Practice?_, Operations Research 2020

---

## License

This project is distributed under the [Creative Commons Attribution 4.0 International License (CC-BY-4.0)](https://creativecommons.org/licenses/by/4.0/), consistent with the original paper's open access license.
