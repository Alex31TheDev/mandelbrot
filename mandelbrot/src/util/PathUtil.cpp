#include "PathUtil.h"

#include <cstdlib>
#include <ctime>

#include <string>
#include <chrono>
using namespace std::chrono;

namespace PathUtil {
    std::string appendIsoDate(const std::string &filename) {
        std::string name, ext;
        size_t pos = filename.rfind('.');

        if (pos == std::string::npos) {
            name = filename;
            ext = "";
        } else {
            name = filename.substr(0, pos);
            ext = filename.substr(pos);
        }

        time_t now = system_clock::to_time_t(system_clock::now());
        std::tm *localTime = std::localtime(&now);

        char buf[20];
        strftime(buf, sizeof(buf), "%Y_%m_%d-%H_%M_%S", localTime);

        return name + "-" + buf + ext;
    }

    std::string getAbsolutePath(const std::string &filename) {
#ifdef _WIN32
        char absPath[_MAX_PATH] = { 0 };

        if (_fullpath(absPath, filename.c_str(), _MAX_PATH) != nullptr) {
            return absPath;
        }
#else
        char *resolved = realpath(filename.c_str(), nullptr);

        if (resolved != nullptr) {
            std::string result(resolved);
            free(resolved);

            return result;
        }
#endif
        return filename;
    }
}