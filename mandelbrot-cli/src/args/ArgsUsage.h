#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "util/RangeUtil.h"

namespace ArgsUsage {
    DECLARE_RANGE_ARRAY(char *, helpOptions);

    extern const char *replOption;
    extern const char *exitOption;
    extern const char *skipOption;

    extern const char *flagHelp;
    extern const char *modeHelp;

    extern const std::unordered_map<std::string, std::string> backendAliases;

    [[nodiscard]] inline int argsCount(int argc) { return argc - 1; }

    bool isHelpArg(const char *arg);
    void printTopLevelUsage(const char *progName);
    bool printDetailedHelp(int argc, char **argv);
    std::string resolveBackend(std::string_view input);
}
