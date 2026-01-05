#include "lazymc/mcqd_adapter.h"

#include "mcqd.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lazymc {

namespace {

struct ConnMatrix {
    bool** conn = nullptr;
    int n = 0;

    explicit ConnMatrix(int n_) : n(n_) {
        conn = new bool*[n];
        for (int i = 0; i < n; ++i) {
            conn[i] = new bool[n];
            std::memset(conn[i], 0, static_cast<std::size_t>(n) * sizeof(bool));
        }
    }

    ConnMatrix(const ConnMatrix&) = delete;
    ConnMatrix& operator=(const ConnMatrix&) = delete;

    ~ConnMatrix() {
        if (!conn) return;
        for (int i = 0; i < n; ++i) delete[] conn[i];
        delete[] conn;
    }
};

} // namespace

McqdResult mcqd_max_clique(const AdjList& G, double dyn_fraction) {
    McqdResult out;

    const int n = static_cast<int>(G.size());
    if (n <= 0) return out;

    // Safety: MCQD requires O(n^2) memory for bool matrix
    // 10,000 -> 100M booleans ~ 100MB+ (and overhead), can be too much on Windows
    constexpr int kMaxNDefault = 6000;
    if (n > kMaxNDefault) {
        throw std::runtime_error(
            "MCQD adapter: graph too large for bool[n][n] adjacency matrix (n="
            + std::to_string(n) + "). Run with --no-mcqd or lower n.");
    }

    ConnMatrix M(n);

    // Fill adjacency
    for (int v = 0; v < n; ++v) {
        for (int32_t u_i32 : G[static_cast<std::size_t>(v)]) {
            const int u = static_cast<int>(u_i32);
            if (u < 0 || u >= n) continue;
            if (u == v) continue;
            M.conn[v][u] = true;
            M.conn[u][v] = true; // undirected
        }
    }

    int* qmax = nullptr;
    int qsize = 0;

    if (dyn_fraction > 0.0) {
        Maxclique solver(M.conn, n, dyn_fraction);
        solver.mcqdyn(qmax, qsize);
        out.steps = solver.steps();
    } else {
        Maxclique solver(M.conn, n);
        solver.mcq(qmax, qsize);
        out.steps = solver.steps();
    }

    out.clique.reserve(static_cast<std::size_t>(qsize));
    for (int i = 0; i < qsize; ++i) {
        out.clique.push_back(static_cast<int32_t>(qmax[i]));
    }
    delete[] qmax;

    return out;
}

} // namespace lazymc
