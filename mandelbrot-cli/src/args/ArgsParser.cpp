#include "ArgsParser.h"

#include <cstdio>
#include <cstdio>
#include <memory>
#include <string_view>
#include <string>
#include <stdexcept>
#include <vector>

#include "BackendAPI.h"
#include "options/ColorMethods.h"
#include "options/FractalTypes.h"
#include "util/ParserUtil.h"

#include "argparse.hpp"

#include "ArgsUsage.h"
#include "palette/PaletteParser.h"
using namespace ArgsUsage;

using namespace ColorMethods;
using namespace FractalTypes;
using namespace ParserUtil;

constexpr float DEFAULT_FREQ_R = 0.98f;
constexpr float DEFAULT_FREQ_G = 0.91f;
constexpr float DEFAULT_FREQ_B = 0.86f;
constexpr float DEFAULT_FREQ_MULT = 0.128f;
constexpr float DEFAULT_LIGHT_R = 1.0f;
constexpr float DEFAULT_LIGHT_I = 1.0f;

struct ParsedArgs {
    int width = 0;
    int height = 0;
    int aaPixels = 1;
    std::string point_r = "0";
    std::string point_i = "0";
    float zoom = 0.0f;
    std::string count = "auto";
    bool useThreads = false;
    std::string colorMethod = DEFAULT_COLOR_METHOD.name;
    bool isJuliaSet = false;
    bool isInverse = false;
    std::string seed_r = "0";
    std::string seed_i = "0";
    std::string fractalType = DEFAULT_FRACTAL_TYPE.name;
    std::string N = "2";
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

    arg.action([](const std::string &str) { return parseNumber<int>(str); });
    arg.store_into(value);
}

static void addArgumentStringFull(
    argparse::ArgumentParser &parser,
    const char *name, std::string &value,
    std::optional<std::string_view> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return str; });
    arg.store_into(value);
}

static void addArgument_H(
    argparse::ArgumentParser &parser,
    const char *name, float &value,
    std::optional<float> defaultValue = std::nullopt
) {
    auto &arg = parser.add_argument(name);
    if (defaultValue) arg.default_value(*defaultValue).skip_token(skipOption);

    arg.action([](const std::string &str) { return parseNumber<float>(str); });
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
    addArgumentStringFull(parser, "point_r", args.point_r, "0");
    addArgumentStringFull(parser, "point_i", args.point_i, "0");
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

    addArgumentStringFull(parser, "seed_r", args.seed_r, "0");
    addArgumentStringFull(parser, "seed_i", args.seed_i, "0");
    addArgumentOptions(
        parser, fractalTypesRange{},
        "fractalType", args.fractalType,
        defaultFractalType
    );
    addArgumentStringFull(parser, "N", args.N, "2");
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
    float parseColorValue(
        const std::string &str,
        float defaultValue
    ) {
        if (str == skipOption) return defaultValue;
        return parseNumber<float>(str, defaultValue);
    }

    bool checkHelp(int argc, char **argv) {
        if (!argv) return false;
        if (printDetailedHelp(argc, argv)) return true;

        ParsedArgs args;
        const auto parser = makeParser(argv[0], args);

        if (argsCount(argc) < 1 || isHelpArg(argv[1])) {
            printf("%s", parser->help().str().c_str());
            return true;
        }

        return false;
    }

    Backend::Status parse(Backend::Session &session, int argc, char **argv) {
        if (!argv) return Backend::Status::failure("No arguments were provided.");

        ParsedArgs args;
        const auto parser = makeParser(argv[0], args);

        try {
            parser->parse_args(argc, argv);
        } catch (const std::exception &err) {
            return Backend::Status::failure(err.what());
        }

        if (auto status = session.setImageSize(args.width,
            args.height, args.aaPixels); !status) return status;

        int iterCount = 0;

        if (args.count != "auto") {
            iterCount = parseNumber<int>(args.count);
        }

        if (auto status = session.setZoom(iterCount, args.zoom); !status) return status;

        session.setUseThreads(args.useThreads);

        const int colorMethod = parseColorMethod(args.colorMethod);
        if (colorMethod < 0) {
            return Backend::Status::failure("Unknown color method.");
        }

        const int fractalType = parseFractalType(args.fractalType);
        if (fractalType < 0) {
            return Backend::Status::failure("Unknown fractal type.");
        }

        session.setFractalMode(args.isJuliaSet, args.isInverse);

        if (auto status = session.setPoint(args.point_r, args.point_i); !status)
            return status;
        if (auto status = session.setSeed(args.seed_r, args.seed_i); !status)
            return status;
        if (auto status = session.setFractalType(
            static_cast<Backend::FractalType>(fractalType)); !status)
            return status;
        if (auto status = session.setFractalExponent(args.N); !status)
            return status;
        if (auto status = session.setColorMethod(
            static_cast<Backend::ColorMethod>(colorMethod)); !status)
            return status;

        switch (colorMethod) {
            case 0:
            case 1:
                return session.setColorFormula(
                    parseColorValue(args.colorArg1, DEFAULT_FREQ_R),
                    parseColorValue(args.colorArg2, DEFAULT_FREQ_G),
                    parseColorValue(args.colorArg3, DEFAULT_FREQ_B),
                    parseColorValue(args.colorArg4, DEFAULT_FREQ_MULT)
                );

            case 2:
            {
                Backend::PaletteHexConfig paletteCfg;
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

                PaletteParser paletteParser(skipOption);
                if (!paletteParser.parse(paletteArgs, paletteCfg, err)) {
                    return Backend::Status::failure(err);
                }

                return session.setPalette(paletteCfg);
            }

            case 3:
            {
                const float real =
                    parseColorValue(args.colorArg1, DEFAULT_LIGHT_R);
                const float imag =
                    parseColorValue(args.colorArg2, DEFAULT_LIGHT_I);

                return session.setLight(real, imag);
            }

            default:
                return Backend::Status::failure("Unsupported color method.");
        }
    }
}