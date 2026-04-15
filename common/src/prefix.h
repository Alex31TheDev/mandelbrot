#pragma once

#include <string>

inline std::string currentPrefix() {
#ifdef _DEBUG
    return "Debug";
#else
    return "Release";
#endif
}
