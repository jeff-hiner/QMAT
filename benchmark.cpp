// Standalone Q-MAT benchmark for comparing 32-bit vs 64-bit performance
// Compile: cl /O2 /EHsc benchmark.cpp /link qmat.lib (or qmat_x86.lib)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <vector>
#include "qmat_wasm_api.h"

struct MAData {
    std::vector<float> centers;  // x,y,z,x,y,z,...
    std::vector<float> radii;
    std::vector<int32_t> edges;  // v0,v1,v0,v1,...
    std::vector<int32_t> faces;  // v0,v1,v2,...
    size_t vertex_count;
    size_t edge_count;
    size_t face_count;
};

bool load_ma(const char* path, MAData& data) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        return false;
    }

    // Read header
    int nv, ne, nf;
    if (fscanf(f, "%d %d %d", &nv, &ne, &nf) != 3) {
        fprintf(stderr, "Invalid header\n");
        fclose(f);
        return false;
    }

    data.vertex_count = nv;
    data.edge_count = ne;
    data.face_count = nf;
    data.centers.resize(nv * 3);
    data.radii.resize(nv);
    data.edges.resize(ne * 2);
    data.faces.resize(nf * 3);

    // Read vertices
    for (int i = 0; i < nv; i++) {
        char type;
        float x, y, z, r;
        if (fscanf(f, " %c %f %f %f %f", &type, &x, &y, &z, &r) != 5 || type != 'v') {
            fprintf(stderr, "Invalid vertex %d\n", i);
            fclose(f);
            return false;
        }
        data.centers[i*3+0] = x;
        data.centers[i*3+1] = y;
        data.centers[i*3+2] = z;
        data.radii[i] = r;
    }

    // Read edges
    for (int i = 0; i < ne; i++) {
        char type;
        int v0, v1;
        if (fscanf(f, " %c %d %d", &type, &v0, &v1) != 3 || type != 'e') {
            fprintf(stderr, "Invalid edge %d\n", i);
            fclose(f);
            return false;
        }
        data.edges[i*2+0] = v0;
        data.edges[i*2+1] = v1;
    }

    // Read faces
    for (int i = 0; i < nf; i++) {
        char type;
        int v0, v1, v2;
        if (fscanf(f, " %c %d %d %d", &type, &v0, &v1, &v2) != 4 || type != 'f') {
            fprintf(stderr, "Invalid face %d\n", i);
            fclose(f);
            return false;
        }
        data.faces[i*3+0] = v0;
        data.faces[i*3+1] = v1;
        data.faces[i*3+2] = v2;
    }

    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    const char* ma_path = "../../python/benchmarks/cache/sasha_initial.ma";
    int target = 300;

    if (argc > 1) ma_path = argv[1];
    if (argc > 2) target = atoi(argv[2]);

    printf("Loading: %s\n", ma_path);

    MAData data;
    if (!load_ma(ma_path, data)) {
        return 1;
    }

    printf("Input: %zu vertices, %zu edges, %zu faces\n",
           data.vertex_count, data.edge_count, data.face_count);
    printf("Target: %d vertices\n", target);
    printf("Pointer size: %zu bytes (%zu-bit)\n", sizeof(void*), sizeof(void*) * 8);

    // Setup input
    WasmMAT input;
    input.centers.ptr = data.centers.data();
    input.centers.len = data.vertex_count;
    input.radii.ptr = data.radii.data();
    input.radii.len = data.vertex_count;
    input.edges.ptr = data.edges.data();
    input.edges.len = data.edge_count;
    input.faces.ptr = data.faces.data();
    input.faces.len = data.face_count;

    // Setup output buffers
    std::vector<float> out_centers(data.vertex_count * 3);
    std::vector<float> out_radii(data.vertex_count);
    std::vector<int32_t> out_edges(data.edge_count * 2);
    std::vector<int32_t> out_faces(data.face_count * 3);

    WasmMATMut output;
    output.centers.ptr = out_centers.data();
    output.centers.len = data.vertex_count;
    output.radii.ptr = out_radii.data();
    output.radii.len = data.vertex_count;
    output.edges.ptr = out_edges.data();
    output.edges.len = data.edge_count;
    output.faces.ptr = out_faces.data();
    output.faces.len = data.face_count;

    QMATParams params;
    params.bb_diagonal = 1.0f;
    params.target_vertices = target;

    // Benchmark
    printf("\nRunning benchmark...\n");
    auto t0 = std::chrono::high_resolution_clock::now();
    int32_t result = qmat_simplify(&input, &params, &output);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    if (result != QMAT_SUCCESS) {
        fprintf(stderr, "qmat_simplify failed: %d\n", result);
        return 1;
    }

    printf("Output: %zu vertices, %zu edges, %zu faces\n",
           output.centers.len, output.edges.len, output.faces.len);
    printf("\nTime: %.3f seconds\n", elapsed);

    return 0;
}
