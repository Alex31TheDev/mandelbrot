#pragma once

#include <string>

namespace PathUtil {
    std::string appendSeqnum(const std::string &filename, int x);
    std::string appendIsoDate(const std::string &filename);
    std::string getAbsolutePath(const std::string &filename);
}
