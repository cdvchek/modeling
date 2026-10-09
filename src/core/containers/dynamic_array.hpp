#pragma once

#include <types>
#include <atomic>
#include <vector>
#include <cassert>

// A number no other structure change has had: copies of an array keep theirs, so two arrays with the same stamp
// hold the same slots (used to tell whether a GPU copy still has the right layout)
inline u64 nextStructureStamp() {
    static std::atomic<u64> next { 1 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

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

    bool operator==(const Handle&) const = default;
    bool operator!=(const Handle&) const = default;
    bool isNull() const { return index == INVALID_INDEX; }
};

// Generational slot array; HandleT defaults to Handle<T> so any type gets its own handle
template <typename T, typename HandleT = Handle<T>>
class DynamicArray {
public:
    HandleT insert(const T& value);

    // Room for count slots in total, so a run of inserts doesn't keep reallocating
    void reserve(u32 count) { m_slots.reserve(count); }

    void remove(HandleT handle);

    bool isValid(HandleT handle) const;

    T& get(HandleT handle);
    const T& get(HandleT handle) const;

    T* tryGet(HandleT handle);
    const T* tryGet(HandleT handle) const;

    HandleT getHandle(u32 index) const;

    std::vector<HandleT> getActiveHandles() const;
    std::vector<T> getActiveValues() const;

    u32 size() const;
    u32 activeSize() const;

    // Changes on every insert and remove (see nextStructureStamp)
    u64 stamp() const { return m_stamp; }

private:
    std::vector<Slot<T>> m_slots;
    std::vector<u32> m_freeSlots;

    u32 m_activeCount = 0;
    u64 m_stamp = nextStructureStamp();
};

template <typename T, typename HandleT>
HandleT DynamicArray<T, HandleT>::insert(const T& value) {
    m_stamp = nextStructureStamp();
    if (!m_freeSlots.empty()) {
        const u32 index = m_freeSlots.back();
        m_freeSlots.pop_back();

        Slot<T>& slot = m_slots[index];
        slot.value = value;
        slot.valid = true;
        ++m_activeCount;

        return HandleT{
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
void DynamicArray<T, HandleT>::remove(HandleT handle) {
    if (!isValid(handle)) return;

    Slot<T>& slot = m_slots[handle.index];
    m_stamp = nextStructureStamp();

    slot.valid = false;

    ++slot.generation;

    m_freeSlots.push_back(handle.index);

    --m_activeCount;
}

template <typename T, typename HandleT>
bool DynamicArray<T, HandleT>::isValid(HandleT handle) const {
    if (handle.index >= m_slots.size()) return false;

    const Slot<T>& slot = m_slots[handle.index];

    return slot.valid && slot.generation == handle.generation;
}

template <typename T, typename HandleT>
T& DynamicArray<T, HandleT>::get(HandleT handle) {
    assert(isValid(handle));
    return m_slots[handle.index].value;
}

template <typename T, typename HandleT>
const T& DynamicArray<T, HandleT>::get(HandleT handle) const {
    assert(isValid(handle));
    return m_slots[handle.index].value;
}

template <typename T, typename HandleT>
T* DynamicArray<T, HandleT>::tryGet(HandleT handle) {
    if (isValid(handle)) return &m_slots[handle.index].value;
    return nullptr;
}

template <typename T, typename HandleT>
const T* DynamicArray<T, HandleT>::tryGet(HandleT handle) const {
    if (isValid(handle)) return &m_slots[handle.index].value;
    return nullptr;
}

template <typename T, typename HandleT>
HandleT DynamicArray<T, HandleT>::getHandle(u32 index) const {
    assert(index < m_slots.size());

    const Slot<T>& slot = m_slots[index];

    if (!slot.valid) {
        return {};
    }

    return HandleT{
        .index = index,
        .generation = slot.generation
    };
}

template <typename T, typename HandleT>
std::vector<HandleT> DynamicArray<T, HandleT>::getActiveHandles() const {
    std::vector<HandleT> handles;
    handles.reserve(m_activeCount);

    for (u32 i = 0; i < m_slots.size(); ++i) {
        const Slot<T>& slot = m_slots[i];

        if (!slot.valid) {
            continue;
        }

        handles.push_back(HandleT{
            .index = i,
            .generation = slot.generation
        });
    }

    return handles;
}

template <typename T, typename HandleT>
std::vector<T> DynamicArray<T, HandleT>::getActiveValues() const {
    std::vector<T> values;
    values.reserve(m_activeCount);

    for (const Slot<T>& slot : m_slots) {
        if (!slot.valid) {
            continue;
        }

        values.push_back(slot.value);
    }

    return values;
}

template <typename T, typename HandleT>
u32 DynamicArray<T, HandleT>::size() const {
    return static_cast<u32>(m_slots.size());
}

template <typename T, typename HandleT>
u32 DynamicArray<T, HandleT>::activeSize() const {
    return m_activeCount;
}