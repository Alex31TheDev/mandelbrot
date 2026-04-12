#include "ArgsParser.h"

#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>

#include "argparse.hpp"

#include "ArgsUsage.h"
#include "../options/ColorMethods.h"
using namespace ArgsUsage;
using namespace ColorMethods;

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
    scalar_full_t N = DEFAULT_FRACTAL_EXP;
    scalar_half_t colorArg1 = ZERO_H;
    scalar_half_t colorArg2 = ZERO_H;
    scalar_half_t colorArg3 = ZERO_H;
    scalar_half_t colorArg4 = ZERO_H;
};

static void addArgumentString(
    argparse::ArgumentParser &parser,
    const char *name, std::string &value,
    std::optional<std::string_view> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);
    arg.store_into(value);
}

template<auto param, typename Range>
static void addArgumentChoices(
    argparse::ArgumentParser &parser,
    Range range,
    const char *name, std::string &value,
    std::optional<std::string_view> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);

    for (const auto &item : range)
        arg.add_choice(std::invoke(param, item));

    arg.choices().store_into(value);
}

static void addArgument_INT32(
    argparse::ArgumentParser &parser,
    const char *name, int &value,
    std::optional<int> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);

    arg.action([&value](const std::string &str) {
        value = PARSE_INT32(str.c_str()); });
}

static void addArgument_F(
    argparse::ArgumentParser &parser,
    const char *name, scalar_full_t &value,
    std::optional<scalar_full_t> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);

    arg.action([&value](const std::string &str) {
        value = PARSE_F(str.c_str()); });
}

static void addArgument_H(
    argparse::ArgumentParser &parser,
    const char *name, scalar_half_t &value,
    std::optional<scalar_half_t> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);

    arg.action([&value](const std::string &str) {
        value = PARSE_H(str.c_str()); });
}

static void addArgumentBool(
    argparse::ArgumentParser &parser,
    const char *name, bool &value,
    std::optional<bool> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue);

    arg.action([&value](std::string_view str) {
        bool ok;
        value = parseBool(str, std::ref(ok));
        if (!ok) throw std::runtime_error(flagHelp);
        });
}

static void configureParser(argparse::ArgumentParser &parser, ParsedArgs &args) {
    parser.add_description(modeHelp);

    addArgument_INT32(parser, "width", args.width);
    addArgument_INT32(parser, "height", args.height);
    addArgument_F(parser, "point_r", args.point_r);
    addArgument_F(parser, "point_i", args.point_i);
    addArgument_H(parser, "zoom", args.zoom);

    addArgumentString(parser, "count", args.count, "auto");
    addArgumentBool(parser, "useThreads", args.useThreads, false);
    addArgumentChoices<&ColorMethod::name>(
        parser, colorMethodsRange{},
        "colorMethod", args.colorMethod,
        DEFAULT_COLOR_METHOD.name
    );

    addArgumentBool(parser, "isJuliaSet", args.isJuliaSet, false);
    addArgumentBool(parser, "isInverse", args.isInverse, false);

    addArgument_F(parser, "seed_r", args.seed_r, DEFAULT_SEED_R);
    addArgument_F(parser, "seed_i", args.seed_i, DEFAULT_SEED_I);
    addArgument_F(parser, "N", args.N, DEFAULT_FRACTAL_EXP);
    addArgument_H(parser, "color1", args.colorArg1, ZERO_H);
    addArgument_H(parser, "color2", args.colorArg2, ZERO_H);
    addArgument_H(parser, "color3", args.colorArg3, ZERO_H);
    addArgument_H(parser, "color4", args.colorArg4, ZERO_H);
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

        if (setImageGlobals(args.width, args.height)) {
            initImageValues();
        } else {
            fprintf(stderr, "Invalid args.\nWidth and height must be > 0.\n");
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

        setFractalType(args.isJuliaSet, args.isInverse);

        setZoomPoints(args.point_r, args.point_i, args.seed_r, args.seed_i);

        if (!setFractalExponent(args.N)) {
            fprintf(stderr, "Invalid args.\nFractal exponent must be > 1.\n");
            return false;
        }

        switch (colorMethod) {
            case 0:
            case 1:
                if (!setColorGlobals(
                    args.colorArg1 ? args.colorArg1 : DEFAULT_FREQ_R,
                    args.colorArg2 ? args.colorArg2 : DEFAULT_FREQ_G,
                    args.colorArg3 ? args.colorArg3 : DEFAULT_FREQ_B,
                    args.colorArg4 ? args.colorArg4 : DEFAULT_FREQ_MULT
                )) {
                    fprintf(stderr, "Invalid args.\n"
                        "Frequency multiplier must be non-zero.\n");
                    return false;
                }
                break;

            case 2:
            {
                const scalar_half_t real =
                    args.colorArg1 ? args.colorArg1 : DEFAULT_LIGHT_R;
                const scalar_half_t imag =
                    args.colorArg2 ? args.colorArg2 : DEFAULT_LIGHT_I;

                if (!setLightGlobals(real, imag)) {
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