#pragma once

#include <types>
#include <vector>
#include <cassert>

template <typename T>
struct Slot {
    T value;
    u32 generation = 0;
    bool valid = false;
};

template <typename Tag>
struct Handle {
    u32 index = INVALID_INDEX;
    u32 generation = 0;
};

struct VertexTag {};
struct EdgeTag {};
struct FaceTag {};

using VertexHandle = Handle<VertexTag>;
using EdgeHandle   = Handle<EdgeTag>;
using FaceHandle   = Handle<FaceTag>;

template <typename T, typename HandleT>
class MeshArray {
public:
    HandleT insert(const T& value);

    void remove(HandleT handle);

    bool isValid(HandleT handle) const;

    T& get(HandleT handle);
    const T& get(HandleT handle) const;

    T* tryGet(HandleT handle);
    const T* tryGet(HandleT handle) const;

    u32 size() const;
    u32 activeSize() const;

private:
    std::vector<Slot<T>> m_slots;
    std::vector<u32> m_freeSlots;

    u32 m_activeCount = 0;
};