#include "lazymc/k_vertex_cover.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Very simple CLI for running k-Vertex-Cover on a graph from a text file.
// Usage: vc_cli <graph_file> <k>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: vc_cli <graph_file> <k>\n";
        return 1;
    }

    const char* filename = argv[1];

    int k = 0;
    {
        std::istringstream iss(argv[2]);
        if (!(iss >> k)) {
            std::cerr << "Invalid k value: " << argv[2] << "\n";
            return 1;
        }
    }

    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return 1;
    }

    int n = 0;
    int m = 0;
    if (!(in >> n >> m)) {
        std::cerr << "Failed to read n and m from file.\n";
        return 1;
    }

    if (n < 0 || m < 0) {
        std::cerr << "Invalid n or m in file.\n";
        return 1;
    }

    lazymc::AdjList G;
    G.assign(n, {});

    for (int i = 0; i < m; ++i) {
        int u = 0, v = 0;
        if (!(in >> u >> v)) {
            std::cerr << "Failed to read edge " << i << " from file.\n";
            return 1;
        }

        if (u < 0 || u >= n || v < 0 || v >= n) {
            std::cerr << "Invalid edge (" << u << ", " << v << ") in file.\n";
            return 1;
        }

        // Undirected graph: add both directions.
        G[u].push_back(v);
        G[v].push_back(u);
    }

    // Run the solver.
    lazymc::k_vertex_cover_res res = lazymc::k_vertex_cover_adj(G, k);

    if (res.aborted) {
        std::cout << "Search aborted (step limit exceeded). "
                  << "No conclusion for k = " << k << ".\n";
        return 0;
    }

    if (!res.found) {
        std::cout << "NO vertex cover of size <= " << k << " found.\n";
        return 0;
    }

    std::cout << "FOUND vertex cover of size " << res.cover.size()
              << " (k = " << k << ")\n";

    std::cout << "Vertices in cover:";
    for (int v : res.cover) {
        std::cout << " " << v;
    }
    std::cout << "\n";

    return 0;
}
