#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace PathUtil {
    std::tuple<std::string_view, std::string_view>
        splitFilename(std::string_view filePath);
    std::string appendSeqnum(std::string_view filePath, int x);
    std::string appendIsoDate(std::string_view filePath);
    std::string getAbsolutePath(const std::string &filePath);
}
