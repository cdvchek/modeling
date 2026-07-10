#include "scene/mesh/mesh_factory.hpp"

MeshData MeshFactory::createCube() {
    MeshData mesh;

    mesh.setVertices({
        { Vec3(-0.5f, -0.5f, -0.5f) },
        { Vec3( 0.5f, -0.5f, -0.5f) },
        { Vec3( 0.5f,  0.5f, -0.5f) },
        { Vec3(-0.5f,  0.5f, -0.5f) },

        { Vec3(-0.5f, -0.5f,  0.5f) },
        { Vec3( 0.5f, -0.5f,  0.5f) },
        { Vec3( 0.5f,  0.5f,  0.5f) },
        { Vec3(-0.5f,  0.5f,  0.5f) }
    });

    mesh.setEdges({
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    });

    Face face1;
    face1.vertices = {
        0, 1, 2,
        0, 2, 3
    };

    Face face2;
    face2.vertices = {
        4, 5, 6,
        4, 6, 7
    };

    Face face3;
    face3.vertices = {
        0, 1, 4,
        4, 5, 1
    };

    Face face4;
    face4.vertices = {
        0, 3, 4,
        3, 4, 7
    };

    Face face5;
    face5.vertices = {
        2, 3, 6,
        3, 6, 7
    };

    Face face6;
    face6.vertices = {
        6, 5, 2,
        5, 2, 1
    };

    std::vector<Face> faces;
    faces.push_back(face1);
    faces.push_back(face2);
    faces.push_back(face3);
    faces.push_back(face4);
    faces.push_back(face5);
    faces.push_back(face6);

    mesh.setFaces(faces);

    return mesh;
}