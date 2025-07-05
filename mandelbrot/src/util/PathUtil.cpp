#include "PathUtil.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <string>
#include <string_view>
#include <tuple>
#include <chrono>

using namespace std::chrono;

static bool safeLocaltime(time_t time, tm &out) {
#if defined(_MSC_VER)
    return localtime_s(&out, &time) == 0;
#else
    return localtime_r(&time, &out) != nullptr;
#endif
}

namespace PathUtil {
    std::tuple<std::string_view, std::string_view>
        splitFilename(std::string_view filePath) {
        std::string_view name, ext;

        const size_t lastDot = filePath.find_last_of('.');
        const size_t lastSlash = filePath.find_last_of("/\\");

        if (lastDot != std::string::npos &&
            lastDot != filePath.length() - 1 &&
            (lastSlash == std::string::npos ||
                lastDot > lastSlash)) {
            name = filePath.substr(0, lastDot);
            ext = filePath.substr(lastDot);
        } else {
            name = filePath;
            ext = "";
        }

        return std::make_tuple(name, ext);
    }

    std::string getAbsolutePath(const std::string &filePath) {
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
            return std::string(result);
        }
#endif
        return filePath;
    }

    std::string appendSeqnum(std::string_view filePath, int x) {
        const auto [name, ext] = splitFilename(filePath);
        const std::string x_str = std::to_string(x);

        std::string result;
        result.reserve(name.size() + x_str.size() + ext.size() + 1);
        result.append(name);
        result.push_back('_');
        result.append(x_str);
        result.append(ext);
        return result;
    }

    std::string appendIsoDate(std::string_view filePath) {
        const auto [name, ext] = splitFilename(filePath);

        const time_t now = system_clock::to_time_t(system_clock::now());
        tm localTime;

        if (!safeLocaltime(now, localTime)) return "";

        char buf[20];
        strftime(buf, sizeof(buf), "%Y_%m_%d-%H_%M_%S", &localTime);

        std::string result;
        result.reserve(name.size() + strlen(buf) + ext.size() + 1);
        result.append(name);
        result.push_back('-');
        result.append(buf);
        result.append(ext);
        return result;
    }
}