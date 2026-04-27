#include "ArgsParser.h"

#include <cstdio>
#include <memory>
#include <string_view>
#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <exception>
#include <stdexcept>

#include "argparse.hpp"
#include "BackendAPI.h"

#include "ArgsUsage.h"
using namespace ArgsUsage;

#include "options/ColorMethods.h"
#include "options/FractalTypes.h"
using namespace ColorMethods;
using namespace FractalTypes;

#include "parsers/palette/PaletteParser.h"
#include "parsers/sine/SineParser.h"

#include "util/ParserUtil.h"
using namespace ParserUtil;

constexpr float DEFAULT_LIGHT_R = 1.0f;
constexpr float DEFAULT_LIGHT_I = 1.0f;
constexpr float DEFAULT_LIGHT_COLOR = 1.0f;

struct ParsedArgs {
    int width = 0;
    int height = 0;
    int aaPixels = 1;
    std::string point_r = "0";
    std::string point_i = "0";
    std::string zoom = "0";
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
    std::vector<std::string> colorArgs;
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
    addArgumentStringFull(parser, "zoom", args.zoom, "0");
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
    parser.add_argument("colorArgs").remaining().store_into(args.colorArgs);
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

static std::vector<std::string> collectColorArgs(const ParsedArgs &args) {
    std::vector<std::string> colorArgs;

    for (const std::string *arg : {
        &args.colorArg1, &args.colorArg2,
        &args.colorArg3, &args.colorArg4
        }) {
        if (*arg != skipOption) colorArgs.push_back(*arg);
    }

    colorArgs.insert(
        colorArgs.end(),
        args.colorArgs.begin(), args.colorArgs.end()
    );
    return colorArgs;
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
            {
                Backend::SinePaletteConfig sineCfg;
                std::string err;

                SineParser sineParser(skipOption);
                if (!sineParser.parse(collectColorArgs(args), sineCfg, err)) {
                    return Backend::Status::failure(err);
                }

                return session.setSinePalette(sineCfg);
            }

            case 2:
            {
                Backend::PaletteHexConfig paletteCfg;
                std::string err;

                PaletteParser paletteParser(skipOption);
                if (!paletteParser.parse(collectColorArgs(args), paletteCfg, err)) {
                    return Backend::Status::failure(err);
                }

                return session.setColorPalette(paletteCfg);
            }

            case 3:
            {
                const float real =
                    parseColorValue(args.colorArg1, DEFAULT_LIGHT_R);
                const float imag =
                    parseColorValue(args.colorArg2, DEFAULT_LIGHT_I);

                if (auto status = session.setLight(real, imag); !status) {
                    return status;
                }

                if (args.colorArg3 != skipOption) {
                    return session.setLightColor(args.colorArg3);
                }

                return session.setLightColor({
                    .R = DEFAULT_LIGHT_COLOR,
                    .G = DEFAULT_LIGHT_COLOR,
                    .B = DEFAULT_LIGHT_COLOR
                    });
            }

            default:
                return Backend::Status::failure("Unsupported color method.");
        }
    }
}