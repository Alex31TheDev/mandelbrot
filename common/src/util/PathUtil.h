#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <tuple>

namespace PathUtil {
    std::filesystem::path executableDir();

    std::tuple<std::string_view, std::string_view> splitFilename(
        std::string_view filePath
    );
    std::string getAbsolutePath(const std::string &filePath);

    std::string appendSeqnum(std::string_view filePath, int x);
    std::string appendIsoDate(std::string_view filePath);
    std::string appendDate(std::string_view filePath, const std::string &format);
    std::string appendExtension(
        std::string_view filePath, const std::string &extension
    );
}
