#include "ParserUtil.h"

#include <cctype>
#include <cstring>

#include <vector>
#include <string>

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
    bool parseBool(const char *str, bool &ok) {
        if (!str) {
            ok = false;
            return false;
        }

        const size_t len = strlen(str);

        if (len != 4 && len != 5) {
            ok = false;
            return false;
        }

        char lower[6] = { 0 };

        for (int i = 0; i < 5 && str[i]; i++) {
            lower[i] = static_cast<char>(tolower(str[i]));
        }

        if (strcmp(lower, "true") == 0) {
            ok = true;
            return true;
        } else if (strcmp(lower, "false") == 0) {
            ok = true;
            return false;
        }

        ok = false;
        return false;
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