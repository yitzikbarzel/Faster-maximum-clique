// Synthetic graph generator for LazyMC benchmarks.
// Output: "n m" header followed by 0-based undirected edges "u v" (deduplicated).
//
// Usage:
//   gen_graph road     <n> <seed>
//   gen_graph chunglu  <n> <avg_deg> <gamma> <num_comm> <comm_min> <comm_max> <comm_p> <planted_clique> <seed>
//   gen_graph rmat     <scale> <edge_factor> <seed>
//   gen_graph gnp      <n> <p> <seed>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using Edge = std::pair<int32_t, int32_t>;

static void emit(int32_t n, std::vector<Edge>& E) {
    for (auto& e : E) if (e.first > e.second) std::swap(e.first, e.second);
    std::sort(E.begin(), E.end());
    E.erase(std::unique(E.begin(), E.end()), E.end());
    E.erase(std::remove_if(E.begin(), E.end(), [](const Edge& e) { return e.first == e.second; }), E.end());
    std::printf("%d %zu\n", n, E.size());
    for (auto& e : E) std::printf("%d %d\n", e.first, e.second);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string kind = argv[1];
    std::vector<Edge> E;

    if (kind == "road") {
        const int32_t n = std::atoi(argv[2]);
        std::mt19937_64 rng(std::strtoull(argv[3], nullptr, 10));
        std::uniform_real_distribution<double> U(0, 1);
        const int32_t side = static_cast<int32_t>(std::sqrt(static_cast<double>(n)));
        const int32_t N = side * side;
        for (int32_t r = 0; r < side; ++r)
            for (int32_t c = 0; c < side; ++c) {
                const int32_t v = r * side + c;
                if (c + 1 < side && U(rng) < 0.65) E.push_back({v, v + 1});
                if (r + 1 < side && U(rng) < 0.65) E.push_back({v, v + side});
                if (r + 1 < side && c + 1 < side && U(rng) < 0.12) E.push_back({v, v + side + 1});
                if (r + 1 < side && c > 0 && U(rng) < 0.05) E.push_back({v, v + side - 1});
            }
        emit(N, E);
    } else if (kind == "chunglu") {
        const int32_t n = std::atoi(argv[2]);
        const double avg = std::atof(argv[3]);
        const double gamma = std::atof(argv[4]);
        const int32_t num_comm = std::atoi(argv[5]);
        const int32_t cmin = std::atoi(argv[6]);
        const int32_t cmax = std::atoi(argv[7]);
        const double cp = std::atof(argv[8]);
        const int32_t planted = std::atoi(argv[9]);
        std::mt19937_64 rng(std::strtoull(argv[10], nullptr, 10));

        // Chung-Lu weights w_i ~ (i + i0)^(-1/(gamma-1))
        std::vector<double> w(static_cast<std::size_t>(n));
        const double ex = -1.0 / (gamma - 1.0);
        for (int32_t i = 0; i < n; ++i) w[static_cast<std::size_t>(i)] = std::pow(i + 10.0, ex);
        std::vector<double> cdf(w.size());
        double s = 0;
        for (std::size_t i = 0; i < w.size(); ++i) { s += w[i]; cdf[i] = s; }
        std::uniform_real_distribution<double> U(0, s);
        auto sample = [&]() {
            const double x = U(rng);
            return static_cast<int32_t>(std::lower_bound(cdf.begin(), cdf.end(), x) - cdf.begin());
        };
        // Random permutation so that hubs are not vertex 0..k
        std::vector<int32_t> perm(static_cast<std::size_t>(n));
        for (int32_t i = 0; i < n; ++i) perm[static_cast<std::size_t>(i)] = i;
        std::shuffle(perm.begin(), perm.end(), rng);

        const std::int64_t m = static_cast<std::int64_t>(avg * n / 2.0);
        E.reserve(static_cast<std::size_t>(m) + 1000000);
        for (std::int64_t e = 0; e < m; ++e) E.push_back({perm[sample()], perm[sample()]});

        // Dense communities (clustering) with members biased towards higher weight.
        std::uniform_real_distribution<double> P(0, 1);
        std::uniform_int_distribution<int32_t> csize(cmin, cmax);
        for (int32_t c = 0; c < num_comm; ++c) {
            const int32_t k = csize(rng);
            std::vector<int32_t> mem;
            while (static_cast<int32_t>(mem.size()) < k) {
                const int32_t v = perm[sample()];
                if (std::find(mem.begin(), mem.end(), v) == mem.end()) mem.push_back(v);
            }
            for (int32_t i = 0; i < k; ++i)
                for (int32_t j = i + 1; j < k; ++j)
                    if (P(rng) < cp) E.push_back({mem[static_cast<std::size_t>(i)], mem[static_cast<std::size_t>(j)]});
        }
        // One planted clique
        if (planted > 0) {
            std::vector<int32_t> mem;
            std::uniform_int_distribution<int32_t> any(0, n - 1);
            while (static_cast<int32_t>(mem.size()) < planted) {
                const int32_t v = any(rng);
                if (std::find(mem.begin(), mem.end(), v) == mem.end()) mem.push_back(v);
            }
            for (int32_t i = 0; i < planted; ++i)
                for (int32_t j = i + 1; j < planted; ++j)
                    E.push_back({mem[static_cast<std::size_t>(i)], mem[static_cast<std::size_t>(j)]});
        }
        emit(n, E);
    } else if (kind == "rmat") {
        const int scale = std::atoi(argv[2]);
        const int ef = std::atoi(argv[3]);
        std::mt19937_64 rng(std::strtoull(argv[4], nullptr, 10));
        std::uniform_real_distribution<double> U(0, 1);
        const int32_t n = 1 << scale;
        const std::int64_t m = static_cast<std::int64_t>(ef) * n;
        const double a = 0.57, b = 0.19, c = 0.19;
        std::vector<int32_t> perm(static_cast<std::size_t>(n));
        for (int32_t i = 0; i < n; ++i) perm[static_cast<std::size_t>(i)] = i;
        std::shuffle(perm.begin(), perm.end(), rng);
        E.reserve(static_cast<std::size_t>(m));
        for (std::int64_t e = 0; e < m; ++e) {
            int32_t u = 0, v = 0;
            for (int bit = scale - 1; bit >= 0; --bit) {
                const double r = U(rng);
                if (r < a) {
                } else if (r < a + b) {
                    v |= 1 << bit;
                } else if (r < a + b + c) {
                    u |= 1 << bit;
                } else {
                    u |= 1 << bit;
                    v |= 1 << bit;
                }
            }
            E.push_back({perm[static_cast<std::size_t>(u)], perm[static_cast<std::size_t>(v)]});
        }
        emit(n, E);
    } else if (kind == "gnp") {
        const int32_t n = std::atoi(argv[2]);
        const double p = std::atof(argv[3]);
        std::mt19937_64 rng(std::strtoull(argv[4], nullptr, 10));
        std::uniform_real_distribution<double> U(0, 1);
        for (int32_t i = 0; i < n; ++i)
            for (int32_t j = i + 1; j < n; ++j)
                if (U(rng) < p) E.push_back({i, j});
        emit(n, E);
    } else {
        return 1;
    }
    return 0;
}
