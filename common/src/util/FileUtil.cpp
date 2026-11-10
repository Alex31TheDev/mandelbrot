#include "FileUtil.h"

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>

#include "TimeUtil.h"
#include "IncludeWin32.h"
#include "StringUtil.h"

namespace FileUtil {
    std::filesystem::path executableDir() {
        std::wstring path(MAX_PATH, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, path.data(),
            static_cast<DWORD>(path.size()));

        path.resize(length);
        return std::filesystem::path(path).parent_path();
    }

    std::tuple<std::string_view, std::string_view> splitFilename(
        std::string_view filePath
    ) {
        std::string_view name, ext;

        const size_t lastDot = filePath.find_last_of('.'),
            lastSlash = filePath.find_last_of("/\\");

        if (lastDot != std::string::npos &&
            lastDot != filePath.length() - 1 &&
            (lastSlash == std::string::npos || lastDot > lastSlash)) {
            name = filePath.substr(0, lastDot);
            ext = filePath.substr(lastDot);
        } else {
            name = filePath;
            ext = "";
        }

        return std::make_tuple(name, ext);
    }

    std::string absolutePath(const std::string &filePath) {
#ifdef _WIN32
        char absPath[_MAX_PATH] = { 0 };

        if (_fullpath(absPath, filePath.c_str(), _MAX_PATH)) {
            return std::string(absPath);
        }
#else
        char *resolved = realpath(filePath.c_str(), nullptr);

        if (resolved) {
            std::string result(resolved);
            free(resolved);
            return result;
        }
#endif
        return filePath;
    }

    std::string appendSeqnum(std::string_view filePath, int seqNum) {
        const auto [name, ext] = splitFilename(filePath);
        const std::string seqStr = std::to_string(seqNum);

        std::string result;
        result.reserve(name.size() + seqStr.size() + ext.size() + 1);
        result.append(name);
        result.push_back('_');
        result.append(seqStr);
        result.append(ext);
        return result;
    }

    std::string appendIsoDate(std::string_view filePath) {
        return appendDate(filePath, "%Y_%m_%d-%H_%M_%S");
    }

    std::string appendDate(
        std::string_view filePath, const std::string &format
    ) {
        const auto [name, ext] = splitFilename(filePath);

        const std::string formatted = TimeUtil::formatCurrentTime(format);
        if (formatted.empty()) return "";

        std::string result;
        result.reserve(name.size() + formatted.size() + ext.size() + 1);
        result.append(name);
        result.push_back('-');
        result.append(formatted);
        result.append(ext);
        return result;
    }

    std::string appendExtension(
        std::string_view filePath, const std::string &extension
    ) {
        std::string_view normalizedExt = extension;
        if (StringUtil::startsWith(normalizedExt, ".")) normalizedExt.remove_prefix(1);
        if (normalizedExt.empty()) return std::string(filePath);

        const std::string lowerPath = StringUtil::toLower(filePath);
        const std::string fullExt = "." + StringUtil::toLower(normalizedExt);

        if (StringUtil::endsWith(lowerPath, fullExt)) return std::string(filePath);

        std::string out;
        out.reserve(filePath.size() + normalizedExt.size() + 1);
        out.append(filePath);
        out.push_back('.');
        out.append(normalizedExt);
        return out;
    }

    std::error_code ensureDirectory(const std::filesystem::path &path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);

        if (!ec && !std::filesystem::is_directory(path, ec)) {
            ec = std::make_error_code(std::errc::not_a_directory);
        }

        return ec;
    }

    bool hideFile(const std::filesystem::path &path) {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) return false;

        return SetFileAttributesW(path.c_str(),
            attributes | FILE_ATTRIBUTE_HIDDEN) != 0;
#else
        (void)path;
        return false;
#endif
    }
}