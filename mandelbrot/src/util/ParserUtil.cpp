#include "ParserUtil.h"

#include <cstdint>
#include <cctype>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>
#include <tuple>

ArgsVec::ArgsVec(int count)
    : argc(count), argv(new char *[count + 1]) {
    argv[count] = nullptr;
}

ArgsVec::~ArgsVec() {
    for (int i = 0; i < argc; i++) delete[] argv[i];
    delete[] argv;
}

ArgsVec ArgsVec::fromParsed(
    char *progName,
    std::vector<std::string> &&parsedArgs
) {
    const int count = static_cast<int>(parsedArgs.size() + 1);
    ArgsVec result(count);

    result.argv[0] = strdup(progName);

    for (size_t i = 0; i < parsedArgs.size(); i++) {
        result.argv[i + 1] = strdup(parsedArgs[i].c_str());
    }

    return result;
}

namespace ParserUtil {
    bool insensitiveCompare(std::string_view str, std::string_view target) {
        if (str.size() != target.size()) return false;

        for (size_t i = 0; i < str.size(); i++) {
            if (tolower(str[i]) != target[i]) return false;
        }

        return true;
    };

    bool parseBool(std::string_view input, bool *ok) {
        bool valid = false;
        bool result = false;

        if (!input.empty()) {
            if (insensitiveCompare(input, "true")) {
                valid = true;
                result = true;
            } else if (insensitiveCompare(input, "false")) {
                valid = true;
                result = false;
            }
        }

        if (ok) *ok = valid;
        return result;
    }

    std::tuple<uint8_t, uint8_t, uint8_t>
        parseHexString(std::string_view str, bool *ok) {
        if (!str.empty() && str[0] == '#') {
            str.remove_prefix(1);
        }

        const bool fullHex = (str.size() == 6);
        const bool shortHex = (str.size() == 3);

        uint8_t r, g, b;
        r = g = b = 0;

        if (!fullHex && !shortHex) {
            if (ok) *ok = false;
            return std::make_tuple(r, g, b);
        }

        const uint32_t rgb = parseNumber<uint32_t, 16>
            (std::string(str), ok, 0);

        if (fullHex) {
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
        } else if (shortHex) {
            r = (rgb >> 8) & 0xF;
            g = (rgb >> 4) & 0xF;
            b = rgb & 0xF;
        }

        return std::make_tuple(r, g, b);
    }

    std::vector<std::string> parseCommandLine(const std::string &cmd) {
        std::vector<std::string> args;
        std::string current;

        bool inQuotes = false, inEscape = false;

        for (const char c : cmd) {
            if (inEscape) {
                current += c;
                inEscape = false;
            } else if (c == '\\') {
                inEscape = true;
            } else if (c == '"') {
                inQuotes = !inQuotes;
            } else if (isspace(c) && !inQuotes) {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }

        if (!current.empty()) args.push_back(current);
        return args;
    }
}