#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "args/Usage.h"
#include "args/ArgsParser.h"

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
    const std::string &filename, int num = -1) {
    const bool appendDate = (num < 0);

    const std::string outName = appendDate
        ? filename
        : PathUtil::appendSeqnum(filename, num);

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

        if (argsCount(parsedArgs.argc) == 1 &&
            strcmp(parsedArgs.argv[1], exitOption) == 0) {
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

    if (argsCount(argc) == 1 &&
        strcmp(argv[1], replOption) == 0) {
        return runRepl(argc, argv);
    } else {
        return runOnce(argc, argv);
    }
}