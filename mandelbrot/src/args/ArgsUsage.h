#pragma once

#include "../util/RangeUtil.h"

namespace ArgsUsage {
    DECLARE_RANGE_ARRAY(char *, helpOptions);

    extern const char *replOption;
    extern const char *exitOption;

    extern const char *flagHelp;
    extern const char *modeHelp;

    [[nodiscard]] inline int argsCount(int argc) { return argc - 1; }

    bool isHelpArg(const char *arg);
    bool printDetailedHelp(int argc, char **argv);
}
