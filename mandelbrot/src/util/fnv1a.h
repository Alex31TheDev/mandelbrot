#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fnv1a {
    constexpr uint32_t val_32 = 0x811c9dc5;
    constexpr uint32_t prime_32 = 0x1000193;

    [[nodiscard]] constexpr uint32_t
        hash_32(std::string_view str) noexcept {
        uint32_t hash = val_32;

        for (const char chr : str) {
            hash ^= static_cast<uint32_t>(chr);
            hash *= prime_32;
        }

        return hash;
    }

    [[nodiscard]] constexpr uint32_t
        hash_32(const char *str) noexcept {
        return hash_32(std::string_view(str));
    }

    [[nodiscard]] constexpr uint32_t
        hash_32(const std::string &str) noexcept {
        return hash_32(std::string_view(str));
    }

    constexpr uint32_t operator""
        _hash_32(const char *str, size_t len) {
        return hash_32(std::string_view(str, len));
    }

    constexpr uint64_t val_64 = 0xcbf29ce484222325;
    constexpr uint64_t prime_64 = 0x100000001b3;

    [[nodiscard]] constexpr uint64_t
        hash_64(std::string_view str) noexcept {
        uint64_t hash = val_64;

        for (const char chr : str) {
            hash ^= static_cast<uint64_t>(chr);
            hash *= prime_64;
        }

        return hash;
    }

    [[nodiscard]] constexpr uint64_t
        hash_64(const char *str) noexcept {
        return hash_64(std::string_view(str));
    }

    [[nodiscard]] constexpr uint64_t
        hash_64(const std::string &str) noexcept {
        return hash_64(std::string_view(str));
    }

    constexpr uint64_t operator""
        _hash_64(const char *str, size_t len) {
        return hash_64(std::string_view(str, len));
    }
}

#define STR_SWITCH(str) switch(fnv1a::hash_32(str))
#define STR_CASE(str) case fnv1a::hash_32(str)
