#include "ParserUtil.h"

#include <cctype>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>

static bool insensitiveCompare(std::string_view str, const char *target) {
    if (str.size() != strlen(target)) return false;

    for (size_t i = 0; i < str.size(); i++) {
        if (tolower(str[i]) != target[i]) return false;
    }

    return true;
};

ArgsVec::ArgsVec(int count)
    : argc(count), argv(new char *[count + 1]) {
    argv[count] = nullptr;
}

ArgsVec::~ArgsVec() {
    for (int i = 0; i < argc; i++) delete[] argv[i];
    delete[] argv;
}

ArgsVec ArgsVec::fromParsed(char *progName,
    std::vector<std::string> &&parsedArgs) {
    const int count = static_cast<int>(parsedArgs.size() + 1);
    ArgsVec result(count);

    result.argv[0] = strdup(progName);

    for (size_t i = 0; i < parsedArgs.size(); i++) {
        result.argv[i + 1] = strdup(parsedArgs[i].c_str());
    }

    return result;
}

namespace ParserUtil {
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

    std::vector<std::string> parseCommandLine(const std::string &cmd) {
        std::vector<std::string> args;
        std::string current;

        bool inQuotes = false;
        bool inEscape = false;

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

        if (!current.empty()) {
            args.push_back(current);
        }

        return args;
    }
}