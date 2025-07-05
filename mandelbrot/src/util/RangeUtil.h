#pragma once

#define DECLARE_RANGE_ARRAY(T, name) \
    extern const T name[]; \
    extern const size_t name##Size; \
    struct name##Range { \
        const T* begin() const; \
        const T* end() const; \
    };

#define DEFINE_RANGE_ARRAY(T, name, ...) \
    const T name[] = { __VA_ARGS__ }; \
    const size_t name##Size = sizeof(name) / sizeof(name[0]); \
    const T* name##Range::begin() const { return name; } \
    const T* name##Range::end() const { return name + name##Size; }