#include "scene/mesh/mesh_array.hpp"

template <typename T, typename HandleT>
HandleT MeshArray<T, HandleT>::insert(const T& value) {
    if (!m_freeSlots.empty()) {
        const u32 index = m_freeSlots.back();
        m_freeSlots.pop_back();

        Slot<T>& slot = m_slots[index];

        slot.value = value;
        slot.valid = true;

        ++m_activeCount;

        return Handle {
            index,
            slot.generation
        };
    }

    const u32 index = static_cast<u32>(m_slots.size());

    m_slots.push_back(Slot<T>{
        value,
        0,
        true
    });

    ++m_activeCount;

    return HandleT{
        index,
        0
    };
}

template <typename T, typename HandleT>
void MeshArray<T, HandleT>::remove(HandleT handle) {
    if (!isValid(handle)) return;

    Slot<T>& slot = m_slots[handle.index];

    slot.valid = false;

    ++slot.generation;

    m_freeSlots.push_back(handle.index);

    --m_activeCount;
}

template <typename T, typename HandleT>
bool MeshArray<T, HandleT>::isValid(HandleT handle) const {
    if (handle.index >= m_slots.size()) return false;

    const Slot<T>& slot = m_slots[handle.index];

    return slot.valid && slot.generation == handle.generation;
}

template <typename T, typename HandleT>
T& MeshArray<T, HandleT>::get(HandleT handle) {
    assert(isValid(handle));
    return m_slots[handle.index].value;
}

template <typename T, typename HandleT>
const T& MeshArray<T, HandleT>::get(HandleT handle) const {
    assert(isValid(handle));
    return m_slots[handle.index].value;
}

template <typename T, typename HandleT>
T* MeshArray<T, HandleT>::tryGet(HandleT handle) {
    if (isValid(handle)) return m_slots[handle.index].value;
    return nullptr;
}

template <typename T, typename HandleT>
const T* MeshArray<T, HandleT>::tryGet(HandleT handle) const {
    if (isValid(handle)) return m_slots[handle.index].value;
    return nullptr;
}

template <typename T, typename HandleT>
u32 MeshArray<T, HandleT>::size() const {
    return static_cast<u32>(m_slots.size());
}

template <typename T, typename HandleT>
u32 MeshArray<T, HandleT>::activeSize() const {
    return m_activeCount;
}

