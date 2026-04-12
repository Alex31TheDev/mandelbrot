#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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
    image = Image::create(RenderGlobals::width, RenderGlobals::height,
        RenderGlobals::useThreads);

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

    int lastWidth = 0, lastHeight = 0;
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

        if (RenderGlobals::width != lastWidth ||
            RenderGlobals::height != lastHeight) {
            if (!initializeImage(image)) continue;
        } else if (image) image->clear();

        renderImage(image.get(), true, true);

        if (saveImage(image.get(), fullname, fileCounter)) {
            lastWidth = RenderGlobals::width;
            lastHeight = RenderGlobals::height;
            fileCounter++;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (ArgsParser::checkHelp(argc, argv)) return 0;

    MPFRGlobals::initMPFR();

    if (ArgsUsage::argsCount(argc) == 1 &&
        std::string_view(argv[1]) == ArgsUsage::replOption) {
        return runRepl(argc, argv);
    } else {
        return runOnce(argc, argv);
    }
}