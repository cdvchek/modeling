#pragma once

#include <vector>
#include "scene/objects/object_collection.hpp"
#include "renderer/opengl/opengl_mesh.hpp"

// GPU copies of each object's mesh, kept outside the scene so objects can be copied freely by undo
class ObjectMeshCache {
public:
    // Creates or rebuilds the object's GPU mesh when it's new or dirty, and clears its dirty flag
    // groupOf puts each face's triangles in its material's run, decided against the materials' stamp. A mesh whose
    // vertices only moved is patched in place; anything else rebuilds it.
    OpenGLMesh* sync(ObjectHandle handle, Object& object, const FaceGroupOf& groupOf, u64 materialsStamp);

    // Frees GPU meshes of objects that no longer exist
    void prune(const ObjectCollection& objects);

private:
    struct Entry {
        bool used = false;
        u32 generation = 0;
        OpenGLMesh mesh;
    };

    std::vector<Entry> m_entries;   // indexed by object slot
};
