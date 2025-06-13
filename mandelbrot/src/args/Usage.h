#pragma once

#include <cstddef>

#define MIN_ARGS 5
#define MAX_ARGS 17

#define _DECLARE_STRING_ARRAY(name) \
    extern const char* name[]; \
    extern const size_t name##Size; \
    struct name##Range { \
        const char** begin() const; \
        const char** end() const; \
    }; \

extern const char *usage;

_DECLARE_STRING_ARRAY(helpOptions);

extern const char *replOption;
extern const char *exitOption;

[[nodiscard]] constexpr int argsCount(int argc) noexcept { return argc - 1; };
void printUsage(const char *progPath = nullptr, bool error = false);
