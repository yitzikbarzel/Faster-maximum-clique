#include "lazymc/bk.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <chrono>


using lazymc::AdjMap;
using lazymc::bron_kerbosch_maximum;

// Load edge list from a text file.
// Each non-empty, non-comment line is "u v".
std::vector<std::pair<int, int>> load_edge_list(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::vector<std::pair<int, int>> edges;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        int u, v;
        if (iss >> u >> v) {
            edges.emplace_back(u, v);
        }
        // malformed lines are ignored
    }

    return edges;
}

// Build AdjMap (map<int, vector<int>>) from edge list (undirected).
AdjMap build_adj_map(const std::vector<std::pair<int, int>>& edges) {
    std::map<int, std::set<int>> tmp;

    for (const auto& e : edges) {
        int u = e.first;
        int v = e.second;
        if (u == v) {
            continue; // ignore self loops
        }
        tmp[u].insert(v);
        tmp[v].insert(u); // undirected
    }

    AdjMap G;
    for (auto& [u, nbrs] : tmp) {
        G[u] = std::vector<int>(nbrs.begin(), nbrs.end());
    }
    return G;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: bk_bench <graph_file>\n";
        return 1;
    }

    const std::string filename = argv[1];

    try {
        std::cout << "Loading graph from: " << filename << "\n";
        auto edges = load_edge_list(filename);
        std::cout << "Read " << edges.size() << " edges\n";

        AdjMap G = build_adj_map(edges);
        std::cout << "Number of vertices (keys in map): " << G.size() << "\n";

        // ---- מדידת זמן ----
        auto t_start = std::chrono::steady_clock::now();
        auto best = bron_kerbosch_maximum(G);
        auto t_end   = std::chrono::steady_clock::now();

        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        // --------------------

        std::cout << "Maximum clique size: " << best.size() << "\n";
        std::cout << "Clique vertices: ";
        for (int v : best) {
            std::cout << v << " ";
        }
        std::cout << "\n";

        std::cout << "Elapsed time: " << elapsed_ms << " ms\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
