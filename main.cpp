#include <iostream>
#include "mcqd.h"
#include <vector>

void run_mcqd_baseline_test() {
    // Small demo graph (replace with your benchmark graphs)
    int size = 5;

    // Allocate adjacency matrix (size x size) and init to false
    bool **conn = new bool*[size];
    for (int i = 0; i < size; ++i) {
        conn[i] = new bool[size]{false};      // מאתחל את כל העמודה ל-false
    }

    // Define a 4-clique on vertices {0,1,2,3}
    std::vector<int> clique_nodes = {0, 1, 2, 3};
    for (size_t i = 0; i < clique_nodes.size(); ++i) {
        for (size_t j = i + 1; j < clique_nodes.size(); ++j) {
            int u = clique_nodes[i];
            int v = clique_nodes[j];
            conn[u][v] = conn[v][u] = true;   // סימטרי כי מטריצת שכנות לבלתי־מכוון
        }
    }

    // Vertex 4 connected only to 0  (תיקון במקום conn[5] = ...)
    conn[4][0] = conn[0][4] = true;

    // Run MCQD
    Maxclique m(conn, size);
    int *qmax = new int[size];
    int qsize = 0;

    // Usually the faster variant
    m.mcqdyn(qmax, qsize);

    std::cout << "MCQD Baseline Result: Max Clique Size = " << qsize << std::endl;

    // Cleanup (תיקון: delete[] למערכים)
    delete[] qmax;
    for (int i = 0; i < size; ++i) delete[] conn[i];
    delete[] conn;
}

int main() {
    run_mcqd_baseline_test();
    return 0;
}
