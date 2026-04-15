#include "ArgsParser.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <stdexcept>
#include <vector>

#include "argparse.hpp"

#include "ArgsUsage.h"
#include "PaletteParser.h"
using namespace ArgsUsage;

#include "../options/ColorMethods.h"
#include "../options/FractalTypes.h"
using namespace ColorMethods;
using namespace FractalTypes;

#include "../scalar/ScalarTypes.h"

#include "../render/RenderGlobals.h"
#include "../scalar/ScalarGlobals.h"
#include "../vector/VectorGlobals.h"
#include "../mpfr/MPFRGlobals.h"
using namespace RenderGlobals;
using namespace ScalarGlobals;

#include "../util/ParserUtil.h"
using namespace ParserUtil;

struct ParsedArgs {
    int width = 0;
    int height = 0;
    int aaPixels = 1;
    scalar_full_t point_r = ZERO_F;
    scalar_full_t point_i = ZERO_F;
    scalar_half_t zoom = ZERO_H;
    std::string count = "auto";
    bool useThreads = false;
    std::string colorMethod = DEFAULT_COLOR_METHOD.name;
    bool isJuliaSet = false;
    bool isInverse = false;
    scalar_full_t seed_r = DEFAULT_SEED_R;
    scalar_full_t seed_i = DEFAULT_SEED_I;
    std::string fractalType = DEFAULT_FRACTAL_TYPE.name;
    scalar_full_t N = DEFAULT_FRACTAL_EXP;
    std::string colorArg1 = skipOption;
    std::string colorArg2 = skipOption;
    std::string colorArg3 = skipOption;
    std::string colorArg4 = skipOption;
    std::vector<std::string> paletteArgs;
};

static void addArgumentString(
    argparse::ArgumentParser &parser,
    const char *name, std::string &value,
    std::optional<std::string_view> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return str; });
    arg.store_into(value);
}

template<typename Range>
static void addArgumentOptions(
    argparse::ArgumentParser &parser,
    Range range,
    const char *name, std::string &value,
    std::optional<std::string_view> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) {
        arg.default_value(*defaultValue)
            .skip_token(skipOption)
            .add_choice(skipOption);
    }

    for (const auto &item : range)
        arg.add_choice(item.name);

    arg.choices().action([](const std::string &str) { return str; });
    arg.store_into(value);
}

static void addArgument_INT32(
    argparse::ArgumentParser &parser,
    const char *name, int &value,
    std::optional<int> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return PARSE_INT32(str.c_str()); });
    arg.store_into(value);
}

static void addArgument_F(
    argparse::ArgumentParser &parser,
    const char *name, scalar_full_t &value,
    std::optional<scalar_full_t> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return PARSE_F(str.c_str()); });
    arg.store_into(value);
}

static void addArgument_H(
    argparse::ArgumentParser &parser,
    const char *name, scalar_half_t &value,
    std::optional<scalar_half_t> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return PARSE_H(str.c_str()); });
    arg.store_into(value);
}

static void addArgumentBool(
    argparse::ArgumentParser &parser,
    const char *name, bool &value,
    std::optional<bool> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) {
        bool ok;
        const bool value = parseBool(str, std::ref(ok));
        if (!ok) throw std::runtime_error(flagHelp);
        return value;
        });
    arg.store_into(value);
}

static void configureParser(argparse::ArgumentParser &parser, ParsedArgs &args) {
    parser.add_description(modeHelp);

    const std::string defaultCount = "auto";
    const std::string defaultColorMethod = DEFAULT_COLOR_METHOD.name;
    const std::string defaultFractalType = DEFAULT_FRACTAL_TYPE.name;

    addArgument_INT32(parser, "width", args.width);
    addArgument_INT32(parser, "height", args.height);
    addArgument_F(parser, "point_r", args.point_r);
    addArgument_F(parser, "point_i", args.point_i);
    addArgument_H(parser, "zoom", args.zoom);
    addArgumentString(parser, "count", args.count, defaultCount);
    addArgument_INT32(parser, "AA", args.aaPixels, 1);
    addArgumentBool(parser, "useThreads", args.useThreads, false);
    addArgumentOptions(
        parser, colorMethodsRange{},
        "colorMethod", args.colorMethod,
        defaultColorMethod
    );

    addArgumentBool(parser, "isJuliaSet", args.isJuliaSet, false);
    addArgumentBool(parser, "isInverse", args.isInverse, false);

    addArgument_F(parser, "seed_r", args.seed_r, DEFAULT_SEED_R);
    addArgument_F(parser, "seed_i", args.seed_i, DEFAULT_SEED_I);
    addArgumentOptions(
        parser, fractalTypesRange{},
        "fractalType", args.fractalType,
        defaultFractalType
    );
    addArgument_F(parser, "N", args.N, DEFAULT_FRACTAL_EXP);
    addArgumentString(parser, "color1", args.colorArg1, skipOption);
    addArgumentString(parser, "color2", args.colorArg2, skipOption);
    addArgumentString(parser, "color3", args.colorArg3, skipOption);
    addArgumentString(parser, "color4", args.colorArg4, skipOption);
    parser.add_argument("paletteArgs").remaining().store_into(args.paletteArgs);
}

static std::unique_ptr<argparse::ArgumentParser> makeParser(
    const char *progName, ParsedArgs &args
) {
    auto parser = std::make_unique<argparse::ArgumentParser>(
        progName ? progName : "mandelbrot",
        "",
        argparse::default_arguments::none,
        false
    );

    configureParser(*parser, args);
    return parser;
}

namespace ArgsParser {
    scalar_half_t parseColorValue(
        const std::string &str,
        scalar_half_t defaultValue
    ) {
        if (str == skipOption) return defaultValue;
        return PARSE_H(str.c_str());
    }

    bool checkHelp(int argc, char **argv) {
        if (!argv) return false;
        if (printDetailedHelp(argc, argv)) return true;

        ParsedArgs args;
        const auto parser = makeParser(argv[0], args);

        if (argsCount(argc) < 1 || isHelpArg(argv[1])) {
            std::cout << parser->help().str();
            return true;
        }

        return false;
    }

    bool parse(int argc, char **argv) {
        if (!argv) return false;

        ParsedArgs args;
        const auto parser = makeParser(argv[0], args);

        try {
            parser->parse_args(argc, argv);
        } catch (const std::exception &err) {
            fprintf(stderr, "%s\n", err.what());
            return false;
        }

        if (!setImageGlobals(args.width, args.height, args.aaPixels)) {
            fprintf(stderr, "Invalid args.\n"
                "Width, height, and aaPixels must be > 0.\n");
            return false;
        }

        int iterCount = 0;

        if (args.count != "auto") {
            iterCount = PARSE_INT32(args.count.c_str());
        }

        if (!setZoomGlobals(iterCount, args.zoom)) {
            fprintf(stderr, "Invalid args.\nScale must be > -3.25.\n");
            return false;
        }

        useThreads = args.useThreads;

        colorMethod = parseColorMethod(args.colorMethod);
        if (colorMethod < 0) return false;

        fractalType = parseFractalType(args.fractalType);
        if (fractalType < 0) return false;

        setFractalType(args.isJuliaSet, args.isInverse);

        setZoomPoints(args.point_r, args.point_i, args.seed_r, args.seed_i);

        if (!setFractalExponent(args.N)) {
            fprintf(stderr, "Invalid args.\nFractal exponent must be > 1.\n");
            return false;
        }

        bool ok = true;

        switch (colorMethod) {
            case 0:
            case 1:
                ok = setColorGlobals(
                    parseColorValue(args.colorArg1, DEFAULT_FREQ_R),
                    parseColorValue(args.colorArg2, DEFAULT_FREQ_G),
                    parseColorValue(args.colorArg3, DEFAULT_FREQ_B),
                    parseColorValue(args.colorArg4, DEFAULT_FREQ_MULT)
                );

                if (!ok) {
                    fprintf(stderr, "Invalid args.\n"
                        "Frequency multiplier must be non-zero.\n");
                    return false;
                }
                break;

            case 2:
            {
                PaletteParser::PaletteConfig paletteCfg;
                std::string err;

                std::vector<std::string> paletteArgs;
                for (const std::string *arg : {
                    &args.colorArg1, &args.colorArg2,
                    &args.colorArg3, &args.colorArg4
                    }) {
                    if (*arg != skipOption) paletteArgs.push_back(*arg);
                }

                paletteArgs.insert(
                    paletteArgs.end(),
                    args.paletteArgs.begin(), args.paletteArgs.end()
                );

                ok = PaletteParser::parse(paletteArgs, paletteCfg, err);

                if (ok) ok = setPaletteGlobals(
                    paletteCfg.entries,
                    paletteCfg.totalLength,
                    paletteCfg.offset
                );

                if (!ok) {
                    fprintf(stderr, "Invalid args.\n%s\n", err.c_str());
                    return false;
                }
                break;
            }

            case 3:
            {
                const scalar_half_t real =
                    parseColorValue(args.colorArg1, DEFAULT_LIGHT_R);
                const scalar_half_t imag =
                    parseColorValue(args.colorArg2, DEFAULT_LIGHT_I);

                ok = setLightGlobals(real, imag);
                if (!ok) {
                    fprintf(stderr, "Invalid args.\n"
                        "Light vector must be non-zero.\n");
                    return false;
                }
                break;
            }

            default:
                return false;
        }

        VectorGlobals::initVectors();
        MPFRGlobals::initMPFRValues(argv[3], argv[4]);

        return true;
    }
}
