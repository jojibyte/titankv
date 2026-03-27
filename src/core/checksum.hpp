#pragma once

#include <cstddef>
#include <cstdint>

namespace titan {

inline constexpr uint32_t kFnv1a32Offset = 2166136261u;
inline constexpr uint32_t kFnv1a32Prime = 16777619u;

inline uint32_t fnv1a32Update(uint32_t hash, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnv1a32Prime;
    }
    return hash;
}

inline uint32_t fnv1a32(const void* data, size_t size) {
    return fnv1a32Update(kFnv1a32Offset, data, size);
}

} // namespace titan
