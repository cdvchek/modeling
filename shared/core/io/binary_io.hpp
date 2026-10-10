#pragma once

#include <bit>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#include <types>

// Files are written in the machine's byte order, which must be little-endian so they read the same everywhere
static_assert(std::endian::native == std::endian::little, "binary files assume a little-endian machine");

// Appends plain values to a growing byte buffer
class BinaryWriter {
public:
    template <typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        writeBytes(&value, sizeof(T));
    }

    template <typename T>
    void writeArray(const T* values, std::size_t count) {
        static_assert(std::is_trivially_copyable_v<T>);
        writeBytes(values, count * sizeof(T));
    }

    // A u32 length, then the characters
    void writeString(const std::string& text) {
        write(static_cast<u32>(text.size()));
        writeBytes(text.data(), text.size());
    }

    void writeBytes(const void* data, std::size_t size) {
        if (size == 0) return;
        const std::size_t at = m_bytes.size();
        m_bytes.resize(at + size);
        std::memcpy(m_bytes.data() + at, data, size);
    }

    void reserve(std::size_t size) { m_bytes.reserve(size); }
    std::size_t size() const { return m_bytes.size(); }
    std::vector<u8>& bytes() { return m_bytes; }
    const std::vector<u8>& bytes() const { return m_bytes; }

private:
    std::vector<u8> m_bytes;
};

// Reads values back from a byte range; a read past the end fails and every later read fails too
class BinaryReader {
public:
    BinaryReader(const u8* data, std::size_t size) : m_data(data), m_size(size) {}

    template <typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        return readBytes(&value, sizeof(T));
    }

    template <typename T>
    bool readArray(T* values, std::size_t count) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (count > remaining() / sizeof(T)) return fail();
        return readBytes(values, count * sizeof(T));
    }

    template <typename T>
    bool readVector(std::vector<T>& values, std::size_t count) {
        if (count > remaining() / sizeof(T)) return fail();
        values.resize(count);
        return readArray(values.data(), count);
    }

    bool readString(std::string& text) {
        u32 length = 0;
        if (!read(length) || length > remaining()) return fail();
        text.assign(reinterpret_cast<const char*>(m_data + m_position), length);
        m_position += length;
        return true;
    }

    bool readBytes(void* out, std::size_t size) {
        if (m_failed || size > remaining()) return fail();
        if (size > 0) std::memcpy(out, m_data + m_position, size);
        m_position += size;
        return true;
    }

    bool ok() const { return !m_failed; }
    std::size_t position() const { return m_position; }
    std::size_t remaining() const { return m_failed ? 0 : m_size - m_position; }

private:
    bool fail() {
        m_failed = true;
        return false;
    }

    const u8* m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
    bool m_failed = false;
};
