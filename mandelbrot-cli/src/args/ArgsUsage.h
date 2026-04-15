#pragma once

#include <string>
#include <string_view>

#include "util/RangeUtil.h"

namespace ArgsUsage {
    DECLARE_RANGE_ARRAY(char *, helpOptions);

    extern const char *replOption;
    extern const char *exitOption;
    extern const char *skipOption;

    extern const char *flagHelp;
    extern const char *modeHelp;

    [[nodiscard]] inline int argsCount(int argc) { return argc - 1; }

    bool isHelpArg(const char *arg);
    void printTopLevelUsage(const char *argv0);
    bool printDetailedHelp(int argc, char **argv);
    std::string resolveVariant(std::string_view input);
}
