#include "lazymc/graph_processing.h"
#include <set>

std::map<int, std::vector<int>>
graph_processing(const std::vector<std::pair<int,int>>& edges)
{
    std::map<int, std::set<int>> adj;
    for (auto& e : edges) {
        const int u = e.first;
        const int v = e.second;
        if (u == v) continue;     // ignore self-loops
        adj[u].insert(v);
        adj[v].insert(u);         // undirected
    }

    std::map<int, std::vector<int>> out;
    for (auto& [u, nbrs] : adj) {
        out[u] = std::vector<int>(nbrs.begin(), nbrs.end()); // sorted & unique
    }
    return out;
}