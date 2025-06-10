#pragma once

#include <string>

namespace PathUtil {
    std::string appendIsoDate(const std::string &filename);
    std::string getAbsolutePath(const std::string &filename);
}
