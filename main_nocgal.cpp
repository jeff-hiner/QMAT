// CGAL-free Q-MAT simplification main
// Compile with: -DQMAT_NO_CGAL

#ifndef QMAT_NO_CGAL
#define QMAT_NO_CGAL
#endif

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>

#include "src/SlabMesh.h"

// Load MA file directly into SlabMesh
bool loadMA(SlabMesh* slabMesh, const std::string& maname, double bb_diagonal) {
    std::ifstream mastream(maname.c_str());
    if (!mastream.is_open()) {
        std::cerr << "Cannot open MA file: " << maname << std::endl;
        return false;
    }

    slabMesh->numVertices = 0;
    slabMesh->numEdges = 0;
    slabMesh->numFaces = 0;
    slabMesh->bound_weight = 0.1;
    slabMesh->bb_diagonal_length = bb_diagonal;

    int nv, ne, nf;
    mastream >> nv >> ne >> nf;

    std::cout << "Loading MA: " << nv << " vertices, " << ne << " edges, " << nf << " faces" << std::endl;

    // Load vertices
    for (int i = 0; i < nv; i++) {
        char ch;
        double x, y, z, r;
        mastream >> ch >> x >> y >> z >> r;

        Bool_SlabVertexPointer bsvp;
        bsvp.first = true;
        bsvp.second = new SlabVertex;
        bsvp.second->sphere.center[0] = x / bb_diagonal;
        bsvp.second->sphere.center[1] = y / bb_diagonal;
        bsvp.second->sphere.center[2] = z / bb_diagonal;
        bsvp.second->sphere.radius = r / bb_diagonal;
        bsvp.second->index = slabMesh->vertices.size();
        slabMesh->vertices.push_back(bsvp);
        slabMesh->numVertices++;
    }

    // Load edges
    for (int i = 0; i < ne; i++) {
        char ch;
        unsigned ver[2];
        mastream >> ch >> ver[0] >> ver[1];

        Bool_SlabEdgePointer bsep;
        bsep.first = true;
        bsep.second = new SlabEdge;
        bsep.second->vertices_.first = ver[0];
        bsep.second->vertices_.second = ver[1];
        slabMesh->vertices[ver[0]].second->edges_.insert(slabMesh->edges.size());
        slabMesh->vertices[ver[1]].second->edges_.insert(slabMesh->edges.size());
        bsep.second->index = slabMesh->edges.size();
        slabMesh->edges.push_back(bsep);
        slabMesh->numEdges++;
    }

    // Load faces
    for (int i = 0; i < nf; i++) {
        char ch;
        unsigned vid[3];
        mastream >> ch >> vid[0] >> vid[1] >> vid[2];

        Bool_SlabFacePointer bsfp;
        bsfp.first = true;
        bsfp.second = new SlabFace;
        bsfp.second->vertices_.insert(vid[0]);
        bsfp.second->vertices_.insert(vid[1]);
        bsfp.second->vertices_.insert(vid[2]);

        unsigned eid[3];
        if (slabMesh->Edge(vid[0], vid[1], eid[0]))
            bsfp.second->edges_.insert(eid[0]);
        if (slabMesh->Edge(vid[0], vid[2], eid[1]))
            bsfp.second->edges_.insert(eid[1]);
        if (slabMesh->Edge(vid[1], vid[2], eid[2]))
            bsfp.second->edges_.insert(eid[2]);

        bsfp.second->index = slabMesh->faces.size();
        slabMesh->vertices[vid[0]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->vertices[vid[1]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->vertices[vid[2]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->edges[eid[0]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->edges[eid[1]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->edges[eid[2]].second->faces_.insert(slabMesh->faces.size());
        slabMesh->faces.push_back(bsfp);
        slabMesh->numFaces++;
    }

    slabMesh->iniNumVertices = slabMesh->numVertices;
    slabMesh->iniNumEdges = slabMesh->numEdges;
    slabMesh->iniNumFaces = slabMesh->numFaces;

    slabMesh->CleanIsolatedVertices();
    slabMesh->computebb();
    slabMesh->ComputeFacesCentroid();
    slabMesh->ComputeFacesNormal();
    slabMesh->ComputeVerticesNormal();
    slabMesh->ComputeEdgesCone();
    slabMesh->ComputeFacesSimpleTriangles();
    slabMesh->DistinguishVertexType();

    return true;
}

void initializeSlabMesh(SlabMesh* slabMesh) {
    float k = 0.00001;
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

void simplify(SlabMesh* slabMesh, unsigned num_spheres) {
    slabMesh->CleanIsolatedVertices();
    int threshold = num_spheres;

    slabMesh->Simplify(slabMesh->numVertices - threshold);

    slabMesh->ComputeFacesNormal();
    slabMesh->ComputeVerticesNormal();
    slabMesh->ComputeEdgesCone();
    slabMesh->ComputeFacesSimpleTriangles();

    std::cout << "Simplification done. Vertices: " << slabMesh->numVertices << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <medial_mesh.ma> <num_target_spheres> [bb_diagonal]" << std::endl;
        std::cerr << "  bb_diagonal: optional bounding box diagonal (default 1.0)" << std::endl;
        return 1;
    }

    std::string maname = argv[1];
    unsigned num_spheres = atoi(argv[2]);
    double bb_diagonal = (argc > 3) ? atof(argv[3]) : 1.0;

    std::cout << "Q-MAT Simplification (CGAL-free)" << std::endl;
    std::cout << "Input: " << maname << std::endl;
    std::cout << "Target spheres: " << num_spheres << std::endl;
    std::cout << "BB diagonal: " << bb_diagonal << std::endl;

    SlabMesh slabMesh;

    if (!loadMA(&slabMesh, maname, bb_diagonal)) {
        return 1;
    }

    initializeSlabMesh(&slabMesh);
    simplify(&slabMesh, num_spheres);

    slabMesh.Export("simplified");

    std::cout << "Output written to simplified___.ma" << std::endl;

    return 0;
}
