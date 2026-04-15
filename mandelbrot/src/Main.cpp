#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <libcpuid/libcpuid.h>

#include "args/ArgsParser.h"
#include "args/ArgsUsage.h"

#include "util/ParserUtil.h"
#include "util/PathUtil.h"

#include "render/RenderGlobals.h"
#include "mpfr/MPFRGlobals.h"

#include "image/Image.h"
#include "render/RenderImage.h"

#include "fullname.h"

static bool initializeImage(std::unique_ptr<Image> &image) {
    image = Image::create(RenderGlobals::outputWidth, RenderGlobals::outputHeight,
        RenderGlobals::useThreads, RenderGlobals::aaPixels);

    return image != nullptr;
}

static bool saveImage(const Image *image,
    const std::string &filename,
    std::optional<int> num = std::nullopt
) {
    const bool appendDate = !num;

    const std::string outName = appendDate
        ? filename
        : PathUtil::appendSeqnum(filename, *num);

    return image->saveToFile(outName, appendDate, OUT_FILETYPE);
}

static int runOnce(int argc, char **argv) {
    if (!ArgsParser::parse(argc, argv)) return 1;

    std::unique_ptr<Image> image;
    if (!initializeImage(image)) return 1;

    renderImage(image.get(), true, true);

    return !saveImage(image.get(), fullname, 1);
}

static int runRepl(int argc, char **argv) {
    bool running = true;

    int lastOutputW = 0, lastOutputH = 0, lastAAPixels = 0;
    std::unique_ptr<Image> image = nullptr;
    int fileCounter = 1;

    while (running) {
        std::string line;

        if (!std::getline(std::cin, line)) {
            running = false;
            continue;
        }

        if (line.length() == 0) continue;

        ArgsVec parsedArgs = ArgsVec::fromParsed(
            argv[0],
            ParserUtil::parseCommandLine(line)
        );

        if (ArgsUsage::argsCount(parsedArgs.argc) == 1 &&
            std::string_view(parsedArgs.argv[1]) == ArgsUsage::exitOption) {
            running = false;
            continue;
        }

        if (ArgsParser::checkHelp(parsedArgs.argc, parsedArgs.argv) ||
            !ArgsParser::parse(parsedArgs.argc, parsedArgs.argv)) {
            continue;
        }

        if (RenderGlobals::outputWidth != lastOutputW ||
            RenderGlobals::outputHeight != lastOutputH ||
            RenderGlobals::aaPixels != lastAAPixels) {
            if (!initializeImage(image)) continue;
        } else if (image) image->clear();

        renderImage(image.get(), true, true);

        if (saveImage(image.get(), fullname, fileCounter)) {
            lastOutputW = RenderGlobals::outputWidth;
            lastOutputH = RenderGlobals::outputHeight;
            lastAAPixels = RenderGlobals::aaPixels;
            fileCounter++;
        }
    }

    return 0;
}

static std::string getCpuName() {
    cpu_raw_data_t raw = {};
    if (cpuid_get_raw_data(&raw) < 0) return cpuid_error();

    cpu_id_t id = {};
    if (cpu_identify(&raw, &id) < 0) return cpuid_error();

    return id.brand_str;
}

int main(int argc, char **argv) {
    if (ArgsParser::checkHelp(argc, argv)) return 0;

    std::cout << "CPU: " << getCpuName() << '\n';
    MPFRGlobals::initMPFR();

    if (ArgsUsage::argsCount(argc) == 1 &&
        std::string_view(argv[1]) == ArgsUsage::replOption) {
        return runRepl(argc, argv);
    } else {
        return runOnce(argc, argv);
    }
}
