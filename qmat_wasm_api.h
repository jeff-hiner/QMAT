// Q-MAT WASM Buffer API
// C-compatible interface for WebAssembly module
//
// Design: Caller allocates all buffers. Each array pointer is paired with
// its element count. Structs group related data logically.

#ifndef QMAT_WASM_API_H
#define QMAT_WASM_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============ Error codes ============

#define QMAT_SUCCESS           0
#define QMAT_ERR_NULL_PTR     -1
#define QMAT_ERR_INVALID_SIZE -2
#define QMAT_ERR_TOPOLOGY     -3
#define QMAT_ERR_SIMPLIFY     -4
#define QMAT_ERR_BUFFER_TOO_SMALL -5

// ============ Slice types (ptr + len always paired) ============

typedef struct {
    const float* ptr;
    size_t len;  // element count (e.g., vertex count), not byte count
} FloatSlice;

typedef struct {
    const int32_t* ptr;
    size_t len;
} Int32Slice;

typedef struct {
    float* ptr;
    size_t len;  // capacity on input, actual count on output
} FloatSliceMut;

typedef struct {
    int32_t* ptr;
    size_t len;  // capacity on input, actual count on output
} Int32SliceMut;

// ============ Domain structures ============

// Medial Axis Transform representation (immutable input)
// Prefixed to avoid conflicts with internal C++ types
typedef struct {
    FloatSlice centers;  // len = vertex_count, data is [x,y,z, x,y,z, ...] (stride 3)
    FloatSlice radii;    // len = vertex_count
    Int32Slice edges;    // len = edge_count, data is [v0,v1, v0,v1, ...] (stride 2)
    Int32Slice faces;    // len = face_count, data is [v0,v1,v2, ...] (stride 3)
} WasmMAT;

// Medial Axis Transform representation (mutable output)
// On input: .len fields contain buffer capacity
// On output: .len fields contain actual element count written
typedef struct {
    FloatSliceMut centers;
    FloatSliceMut radii;
    Int32SliceMut edges;
    Int32SliceMut faces;
} WasmMATMut;

// ============ Parameters ============

typedef struct {
    float bb_diagonal;       // Bounding box diagonal for normalization
    int32_t target_vertices; // Target vertex count after simplification
} QMATParams;

// ============ API ============

// Simplify a Medial Axis Transform using quadratic error metrics.
//
// Parameters:
//   input  - Input MAT data (must be valid, non-NULL)
//   params - Simplification parameters
//   output - Pre-allocated output buffers. On input, .len fields specify
//            buffer capacity. On success, .len fields are updated to
//            actual counts written.
//
// Returns:
//   QMAT_SUCCESS on success
//   QMAT_ERR_NULL_PTR if any required pointer is NULL
//   QMAT_ERR_INVALID_SIZE if input sizes are inconsistent
//   QMAT_ERR_BUFFER_TOO_SMALL if output buffers are too small
//   QMAT_ERR_TOPOLOGY on topology errors
//   QMAT_ERR_SIMPLIFY on simplification failure
//
// Note: Output vertex count will be <= min(target_vertices, input vertex count)
//       Output edge/face counts will be <= input edge/face counts
//       Caller should allocate output buffers with capacity >= input counts
int32_t qmat_simplify(
    const WasmMAT* input,
    const QMATParams* params,
    WasmMATMut* output
);

#ifdef __cplusplus
}
#endif

#endif // QMAT_WASM_API_H
