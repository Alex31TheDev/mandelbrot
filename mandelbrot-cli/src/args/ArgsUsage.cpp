#include "ArgsUsage.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <algorithm>
#include <unordered_map>

#include "util/RangeUtil.h"
#include "util/StringUtil.h"
#include "util/fnv1a.h"

#include "prefix.h"

namespace ArgsUsage {
    DEFINE_RANGE_ARRAY(char *, helpOptions, "-h", "--help", "help");

    const char *replOption = "replio";
    const char *exitOption = "exit";
    const char *skipOption = "-";

    const char *flagHelp = "flag must be \"true\" or \"false\"";
    const char *modeHelp =
        "Mode-specific trailing args: "
        "help iterations | help smooth_iterations | help palette | help light";

    static std::string makeConfigName(const std::string &suffix) {
        return currentPrefix() + " - " + suffix;
    }

    bool isHelpArg(const char *arg) {
        return arg && std::ranges::any_of(helpOptionsRange{},
            [arg](const char *opt) { return std::string_view(arg) == opt; });
    }

    void printTopLevelUsage(const char *progName) {
        const std::string prefix = currentPrefix();

        fprintf(stderr,
            "Usage:\n"
            "  %s <variant> [render args...]\n\n"
            "Bundled variants for this build:\n"
            "  %s - FloatScalar\n"
            "  %s - DoubleScalar\n"
            "  %s - FloatAVX2\n"
            "  %s - DoubleAVX2\n"
            "  %s - MPFR\n"
            "  %s - QD\n\n"
            "Short aliases:\n"
            "  float-scalar, double-scalar, float-avx2, double-avx2, mpfr, qd\n",
            progName ? progName : "mandelbrot-cli",
            prefix.c_str(),
            prefix.c_str(),
            prefix.c_str(),
            prefix.c_str(),
            prefix.c_str(),
            prefix.c_str());
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
                    "  [freq_r] [freq_g] [freq_b] [freqMult]\n"
                    "  or a sine file path as the first trailing arg\n"
                    "Sine files use key=value lines for freqR, freqG, "
                    "freqB, and freqMult\n",
                    mode
                );
            return true;

            STR_CASE("palette") :
                printf(
                    "Color method '%s':\n"
                    "\n"
                    "After N, pass:\n"
                    "  [paletteLength] [paletteOffset] [color1] [color2] ...\n"
                    "  or a palette file path as the first trailing arg\n"
                    "Each color is #RRGGBB or #RRGGBB:length\n"
                    "Palette files use key=value lines, then color=#RRGGBB "
                    "length=value\n",
                    mode
                );
            return true;

            STR_CASE("light") :
                printf(
                    "Color method '%s':\n"
                    "\n"
                    "After N, pass:\n"
                    "  [light_r] [light_i] [#RRGGBB]\n"
                    "The light color is optional and defaults to white.\n",
                    mode
                );
            return true;
        }

        return false;
    }

    std::string resolveVariant(std::string_view input) {
        static const std::unordered_map<std::string, std::string> aliases = {
            { "floatscalar", "FloatScalar" },
            { "float-scalar", "FloatScalar" },
            { "doublescalar", "DoubleScalar" },
            { "double-scalar", "DoubleScalar" },
            { "floatavx2", "FloatAVX2" },
            { "float-avx2", "FloatAVX2" },
            { "doubleavx2", "DoubleAVX2" },
            { "double-avx2", "DoubleAVX2" },
            { "mpfr", "MPFR" },
            { "qd", "QD" }
        };

        const std::string lower = StringUtil::toLower(input);
        if (const auto it = aliases.find(lower); it != aliases.end()) {
            return makeConfigName(it->second);
        }

        const std::string prefixLower = StringUtil::toLower(currentPrefix());
        if (lower.rfind(prefixLower + " - ", 0) == 0) {
            const std::string suffix = lower.substr(prefixLower.size() + 3);
            if (const auto it = aliases.find(suffix); it != aliases.end()) {
                return makeConfigName(it->second);
            }
        }

        return {};
    }
}
