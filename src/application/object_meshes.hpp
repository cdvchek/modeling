#pragma once

#include <vector>
#include "scene/objects/object_collection.hpp"
#include "renderer/opengl/opengl_mesh.hpp"

// GPU copies of each object's mesh, kept outside the scene so objects can be copied freely by undo
class ObjectMeshCache {
public:
    // Creates or rebuilds the object's GPU mesh when it's new or dirty, and clears its dirty flag
    OpenGLMesh* sync(ObjectHandle handle, Object& object);

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
