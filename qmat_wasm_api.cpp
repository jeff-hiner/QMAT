// Q-MAT WASM Buffer API Implementation
#ifndef QMAT_NO_CGAL
#define QMAT_NO_CGAL
#endif

#include "qmat_wasm_api.h"
#include "src/SlabMesh.h"
#include <cstdlib>
#include <cstring>
#include <new>

// Memory management exports
extern "C" {

void* wasm_malloc(size_t size) {
    return malloc(size);
}

void wasm_free(void* ptr) {
    free(ptr);
}

} // extern "C"

// Initialize slab mesh for simplification (from main_nocgal.cpp pattern)
static void initializeSlabMesh(SlabMesh* slabMesh) {
    float k = 0.00001f;
    slabMesh->k = k;
    slabMesh->preserve_boundary_method = 0;
    slabMesh->hyperbolic_weight_type = 3;
    slabMesh->compute_hausdorff = false;
    slabMesh->boundary_compute_scale = 0;
    slabMesh->prevent_inversion = false;

    // Initialize slab quadratic error for each vertex
    for (unsigned i = 0; i < slabMesh->vertices.size(); i++) {
        if (!slabMesh->vertices[i].first) continue;

        SlabVertex& sv = *slabMesh->vertices[i].second;
        std::set<unsigned>& fset = sv.faces_;
        Wm4::Vector4d C1(sv.sphere.center.X(), sv.sphere.center.Y(),
                    sv.sphere.center.Z(), sv.sphere.radius);

        for (auto si = fset.begin(); si != fset.end(); si++) {
            SlabFace& sf = *slabMesh->faces[*si].second;

            if (sf.valid_st == false || sf.st[0].normal == Wm4::Vector3d(0., 0., 0.) ||
                sf.st[1].normal == Wm4::Vector3d(0., 0., 0.))
                continue;

            Wm4::Vector4d normal1(sf.st[0].normal.X(), sf.st[0].normal.Y(),
                             sf.st[0].normal.Z(), 1.0);
            Wm4::Vector4d normal2(sf.st[1].normal.X(), sf.st[1].normal.Y(),
                             sf.st[1].normal.Z(), 1.0);

            Wm4::Matrix4d temp_A1, temp_A2;
            temp_A1.MakeTensorProduct(normal1, normal1);
            temp_A2.MakeTensorProduct(normal2, normal2);
            temp_A1 *= 2.0;
            temp_A2 *= 2.0;

            double normal_mul_point1 = normal1.Dot(C1);
            double normal_mul_point2 = normal2.Dot(C1);
            Wm4::Vector4d temp_b1 = normal1 * 2 * normal_mul_point1;
            Wm4::Vector4d temp_b2 = normal2 * 2 * normal_mul_point2;

            double temp_c1 = normal_mul_point1 * normal_mul_point1;
            double temp_c2 = normal_mul_point2 * normal_mul_point2;

            slabMesh->vertices[i].second->slab_A += temp_A1;
            slabMesh->vertices[i].second->slab_A += temp_A2;
            slabMesh->vertices[i].second->slab_b += temp_b1;
            slabMesh->vertices[i].second->slab_b += temp_b2;
            slabMesh->vertices[i].second->slab_c += temp_c1;
            slabMesh->vertices[i].second->slab_c += temp_c2;

            slabMesh->vertices[i].second->related_face += 2;
        }
    }

    // Boundary preservation
    switch (slabMesh->preserve_boundary_method) {
        case 1:
            slabMesh->PreservBoundaryMethodOne();
            break;
        case 3:
            slabMesh->PreservBoundaryMethodThree();
            break;
        default:
            slabMesh->PreservBoundaryMethodFour();
            break;
    }

    slabMesh->initCollapseQueue();
}

// Perform simplification
static void simplify(SlabMesh* slabMesh, unsigned num_spheres) {
    slabMesh->CleanIsolatedVertices();
    int threshold = num_spheres;

    slabMesh->Simplify(slabMesh->numVertices - threshold);

    slabMesh->ComputeFacesNormal();
    slabMesh->ComputeVerticesNormal();
    slabMesh->ComputeEdgesCone();
    slabMesh->ComputeFacesSimpleTriangles();
}

// Create error result
static void* create_error_result(int32_t error_code) {
    void* result = wasm_malloc(16);  // Header only
    if (result) {
        int32_t* header = static_cast<int32_t*>(result);
        header[0] = error_code;
        header[1] = 0;  // vertex_count
        header[2] = 0;  // edge_count
        header[3] = 0;  // face_count
    }
    return result;
}

extern "C" {

void* qmat_simplify_buffer(
    int32_t vertex_count,
    int32_t edge_count,
    int32_t face_count,
    float bb_diagonal,
    int32_t target_vertices,
    const float* vertex_centers,
    const float* vertex_radii,
    const int32_t* edges,
    const int32_t* faces)
{
    // Validate inputs
    if (vertex_count <= 0 || !vertex_centers || !vertex_radii) {
        return create_error_result(QMAT_ERR_INVALID);
    }
    if (edge_count < 0 || (edge_count > 0 && !edges)) {
        return create_error_result(QMAT_ERR_INVALID);
    }
    if (face_count < 0 || (face_count > 0 && !faces)) {
        return create_error_result(QMAT_ERR_INVALID);
    }
    if (bb_diagonal <= 0 || target_vertices <= 0) {
        return create_error_result(QMAT_ERR_INVALID);
    }

    SlabMesh* slabMesh = nullptr;
    void* result = nullptr;

    try {
        slabMesh = new SlabMesh();

        // Load from buffers
        if (!slabMesh->loadFromBuffers(
                vertex_count,
                vertex_centers,
                vertex_radii,
                edge_count,
                edges,
                face_count,
                faces,
                static_cast<double>(bb_diagonal))) {
            delete slabMesh;
            return create_error_result(QMAT_ERR_TOPOLOGY);
        }

        // Initialize and simplify
        initializeSlabMesh(slabMesh);
        simplify(slabMesh, static_cast<unsigned>(target_vertices));

        // Calculate output size
        size_t header_size = 4 * sizeof(int32_t);
        size_t centers_size = slabMesh->numVertices * 3 * sizeof(float);
        size_t radii_size = slabMesh->numVertices * sizeof(float);
        size_t edges_size = slabMesh->numEdges * 2 * sizeof(int32_t);
        size_t faces_size = slabMesh->numFaces * 3 * sizeof(int32_t);
        size_t total_size = header_size + centers_size + radii_size + edges_size + faces_size;

        // Allocate result buffer
        result = wasm_malloc(total_size);
        if (!result) {
            delete slabMesh;
            return create_error_result(QMAT_ERR_ALLOC);
        }

        // Write header
        int32_t* header = static_cast<int32_t*>(result);
        header[0] = QMAT_SUCCESS;
        header[1] = static_cast<int32_t>(slabMesh->numVertices);
        header[2] = static_cast<int32_t>(slabMesh->numEdges);
        header[3] = static_cast<int32_t>(slabMesh->numFaces);

        // Calculate data pointers
        uint8_t* base = static_cast<uint8_t*>(result);
        float* out_centers = reinterpret_cast<float*>(base + header_size);
        float* out_radii = reinterpret_cast<float*>(base + header_size + centers_size);
        int32_t* out_edges = reinterpret_cast<int32_t*>(base + header_size + centers_size + radii_size);
        int32_t* out_faces = reinterpret_cast<int32_t*>(base + header_size + centers_size + radii_size + edges_size);

        // Export to buffers
        slabMesh->exportToBuffers(out_centers, out_radii, out_edges, out_faces);

        delete slabMesh;
        return result;

    } catch (const std::exception& e) {
        if (slabMesh) delete slabMesh;
        if (result) wasm_free(result);
        return create_error_result(QMAT_ERR_SIMPLIFY);
    } catch (...) {
        if (slabMesh) delete slabMesh;
        if (result) wasm_free(result);
        return create_error_result(QMAT_ERR_SIMPLIFY);
    }
}

// Accessor functions for result buffer
int32_t qmat_result_error_code(void* result) {
    if (!result) return QMAT_ERR_INVALID;
    return static_cast<int32_t*>(result)[0];
}

int32_t qmat_result_vertex_count(void* result) {
    if (!result) return 0;
    return static_cast<int32_t*>(result)[1];
}

int32_t qmat_result_edge_count(void* result) {
    if (!result) return 0;
    return static_cast<int32_t*>(result)[2];
}

int32_t qmat_result_face_count(void* result) {
    if (!result) return 0;
    return static_cast<int32_t*>(result)[3];
}

const float* qmat_result_centers(void* result) {
    if (!result) return nullptr;
    int32_t* header = static_cast<int32_t*>(result);
    if (header[0] != QMAT_SUCCESS) return nullptr;
    return reinterpret_cast<const float*>(header + 4);
}

const float* qmat_result_radii(void* result) {
    if (!result) return nullptr;
    int32_t* header = static_cast<int32_t*>(result);
    if (header[0] != QMAT_SUCCESS) return nullptr;
    int32_t vertex_count = header[1];
    const float* centers = reinterpret_cast<const float*>(header + 4);
    return centers + (vertex_count * 3);
}

const int32_t* qmat_result_edges(void* result) {
    if (!result) return nullptr;
    int32_t* header = static_cast<int32_t*>(result);
    if (header[0] != QMAT_SUCCESS) return nullptr;
    int32_t vertex_count = header[1];
    const float* centers = reinterpret_cast<const float*>(header + 4);
    const float* radii = centers + (vertex_count * 3);
    return reinterpret_cast<const int32_t*>(radii + vertex_count);
}

const int32_t* qmat_result_faces(void* result) {
    if (!result) return nullptr;
    int32_t* header = static_cast<int32_t*>(result);
    if (header[0] != QMAT_SUCCESS) return nullptr;
    int32_t vertex_count = header[1];
    int32_t edge_count = header[2];
    const float* centers = reinterpret_cast<const float*>(header + 4);
    const float* radii = centers + (vertex_count * 3);
    const int32_t* edges = reinterpret_cast<const int32_t*>(radii + vertex_count);
    return edges + (edge_count * 2);
}

} // extern "C"
