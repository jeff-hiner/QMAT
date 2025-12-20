// Q-MAT WASM Buffer API
// C-compatible interface for WebAssembly module
#ifndef QMAT_WASM_API_H
#define QMAT_WASM_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
#define QMAT_SUCCESS          0
#define QMAT_ERR_ALLOC       -1
#define QMAT_ERR_INVALID     -2
#define QMAT_ERR_TOPOLOGY    -3
#define QMAT_ERR_SIMPLIFY    -4

// Result header structure (followed by variable-length data)
// Memory layout:
//   [0-3]   error_code (int32)
//   [4-7]   vertex_count (int32)
//   [8-11]  edge_count (int32)
//   [12-15] face_count (int32)
//   [16...] vertex_centers (float[vertex_count * 3])
//   [...]   vertex_radii (float[vertex_count])
//   [...]   edges (int32[edge_count * 2])
//   [...]   faces (int32[face_count * 3])

// Memory management - must be exported for wasmer-python
void* wasm_malloc(size_t size);
void wasm_free(void* ptr);

// Main simplification function
// Returns pointer to result buffer (caller must wasm_free)
// Returns NULL on critical failure
void* qmat_simplify_buffer(
    int32_t vertex_count,
    int32_t edge_count,
    int32_t face_count,
    float bb_diagonal,
    int32_t target_vertices,
    const float* vertex_centers,    // [vertex_count * 3] - x,y,z interleaved
    const float* vertex_radii,      // [vertex_count]
    const int32_t* edges,           // [edge_count * 2] - v0,v1 pairs
    const int32_t* faces            // [face_count * 3] - v0,v1,v2 triplets
);

// Get counts from result buffer (for convenience)
int32_t qmat_result_error_code(void* result);
int32_t qmat_result_vertex_count(void* result);
int32_t qmat_result_edge_count(void* result);
int32_t qmat_result_face_count(void* result);

// Get data pointers from result buffer
const float* qmat_result_centers(void* result);
const float* qmat_result_radii(void* result);
const int32_t* qmat_result_edges(void* result);
const int32_t* qmat_result_faces(void* result);

#ifdef __cplusplus
}
#endif

#endif // QMAT_WASM_API_H
