// Q-MAT WASM Buffer API Implementation
// Caller-allocated buffer design - no WASM memory management needed

#ifndef QMAT_NO_CGAL
#define QMAT_NO_CGAL
#endif

#include "qmat_wasm_api.h"
#include "src/SlabMesh.h"
#include <cstdlib>
#include <cstring>
#include <new>

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

extern "C" {

// Memory management exports for Python wrapper to allocate WASM memory
void* wasm_malloc(size_t size) {
    return malloc(size);
}

void wasm_free(void* ptr) {
    free(ptr);
}

int32_t qmat_simplify(
    const WasmMAT* input,
    const QMATParams* params,
    WasmMATMut* output)
{
    // Validate null pointers
    if (!input || !params || !output) {
        return QMAT_ERR_NULL_PTR;
    }

    // Validate input data
    size_t vertex_count = input->centers.len;
    size_t edge_count = input->edges.len;
    size_t face_count = input->faces.len;

    if (vertex_count == 0 || !input->centers.ptr || !input->radii.ptr) {
        return QMAT_ERR_INVALID_SIZE;
    }
    if (input->radii.len != vertex_count) {
        return QMAT_ERR_INVALID_SIZE;
    }
    if (edge_count > 0 && !input->edges.ptr) {
        return QMAT_ERR_INVALID_SIZE;
    }
    if (face_count > 0 && !input->faces.ptr) {
        return QMAT_ERR_INVALID_SIZE;
    }

    // Validate parameters
    if (params->bb_diagonal <= 0 || params->target_vertices <= 0) {
        return QMAT_ERR_INVALID_SIZE;
    }

    // Validate output buffer capacities
    if (!output->centers.ptr || output->centers.len < vertex_count) {
        return QMAT_ERR_BUFFER_TOO_SMALL;
    }
    if (!output->radii.ptr || output->radii.len < vertex_count) {
        return QMAT_ERR_BUFFER_TOO_SMALL;
    }
    if (edge_count > 0 && (!output->edges.ptr || output->edges.len < edge_count)) {
        return QMAT_ERR_BUFFER_TOO_SMALL;
    }
    if (face_count > 0 && (!output->faces.ptr || output->faces.len < face_count)) {
        return QMAT_ERR_BUFFER_TOO_SMALL;
    }

    SlabMesh* slabMesh = nullptr;

    try {
        slabMesh = new SlabMesh();

        // Load from input buffers
        if (!slabMesh->loadFromBuffers(
                static_cast<int>(vertex_count),
                input->centers.ptr,
                input->radii.ptr,
                static_cast<int>(edge_count),
                input->edges.ptr,
                static_cast<int>(face_count),
                input->faces.ptr,
                static_cast<double>(params->bb_diagonal))) {
            delete slabMesh;
            return QMAT_ERR_TOPOLOGY;
        }

        // Initialize and simplify
        initializeSlabMesh(slabMesh);
        simplify(slabMesh, static_cast<unsigned>(params->target_vertices));

        // Export to caller-provided buffers
        slabMesh->exportToBuffers(
            output->centers.ptr,
            output->radii.ptr,
            output->edges.ptr,
            output->faces.ptr
        );

        // Update output lengths to actual counts
        output->centers.len = slabMesh->numVertices;
        output->radii.len = slabMesh->numVertices;
        output->edges.len = slabMesh->numEdges;
        output->faces.len = slabMesh->numFaces;

        delete slabMesh;
        return QMAT_SUCCESS;

    } catch (const std::exception& e) {
        if (slabMesh) delete slabMesh;
        return QMAT_ERR_SIMPLIFY;
    } catch (...) {
        if (slabMesh) delete slabMesh;
        return QMAT_ERR_SIMPLIFY;
    }
}

} // extern "C"
