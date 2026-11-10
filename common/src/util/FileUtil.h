#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>

namespace FileUtil {
    std::filesystem::path executableDir();

    std::tuple<std::string_view, std::string_view> splitFilename(
        std::string_view filePath
    );
    std::string absolutePath(const std::string &filePath);

    std::string appendSeqnum(std::string_view filePath, int seqNum);

    std::string appendIsoDate(std::string_view filePath);
    std::string appendDate(std::string_view filePath, const std::string &format);
    std::string appendExtension(
        std::string_view filePath, const std::string &extension
    );

    std::error_code ensureDirectory(const std::filesystem::path &path);
    bool hideFile(const std::filesystem::path &path);
}
