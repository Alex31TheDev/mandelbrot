#include "ArgsUsage.h"

#include <cstdio>
#include <algorithm>

#include "../util/fnv1a.h"

namespace ArgsUsage {
    DEFINE_RANGE_ARRAY(char *, helpOptions, "-h", "--help", "help");

    const char *replOption = "replio";
    const char *exitOption = "exit";

    const char *flagHelp = "flag must be \"true\" or \"false\"";
    const char *modeHelp =
        "Mode-specific trailing args: "
        "help iterations | help smooth_iterations | help light";

    bool isHelpArg(const char *arg) {
        return arg && std::ranges::any_of(helpOptionsRange{},
            [arg](const char *opt) { return std::string_view(arg) == opt; });
    }

    bool printDetailedHelp(int argc, char **argv) {
        if (!argv || argsCount(argc) != 2 || !isHelpArg(argv[1])) {
            return false;
        }

        const char *mode = argv[2];

        STR_SWITCH(mode) {
            STR_CASE("iterations") :
                STR_CASE("smooth_iterations") :
                printf(
                    "Color method '%s':\n"
                    "\n"
                    "After N, pass:\n"
                    "  [freq_r] [freq_g] [freq_b] [freqMult]\n",
                    mode
                );
            return true;

            STR_CASE("light") :
                printf(
                    "Color method '%s':\n"
                    "\n"
                    "After N, pass:\n"
                    "  [light_r] [light_i]\n",
                    mode
                );
            return true;
        }

        return false;
    }
}