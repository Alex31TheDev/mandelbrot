#include <cstring>
#include <iostream>
#include <vector>
#include <memory>

#include "args/Usage.h"
#include "args/ArgsParser.h"

#include "util/ParserUtil.h"
#include "util/PathUtil.h"

#include "render/RenderGlobals.h"
#include "mpfr/MpfrGlobals.h"

#include "image/Image.h"
#include "render/RenderImage.h"

const char filename[] = "mandelbrot.png";

int runOnce(int argc, char **argv) {
    if (!ArgsParser::parse(argc, argv)) return 1;

    auto image = Image::create(RenderGlobals::width, RenderGlobals::height);
    if (image == nullptr) return 1;

    renderImage(image.get());

    if (!image->saveToFile(filename)) return 1;
    return 0;
}

int runRepl(int argc, char **argv) {
    int lastWidth = 0, lastHeight = 0;
    std::unique_ptr<Image> image = nullptr;
    int fileCounter = 1;

    while (true) {
        char line[256];
        if (!std::cin.getline(line, 256)) break;

        auto parsedArgs = ParserUtil::parseCommandLine(line);

        std::vector<char *> argsVec;
        argsVec.reserve(parsedArgs.size() + 1);
        argsVec.push_back(argv[0]);
        for (auto &s : parsedArgs) argsVec.push_back(s.data());

        if (ArgsParser::checkHelp(argsVec.size(), argsVec.data())) {
            continue;
        }

        if (!ArgsParser::parse(argsVec.size(), argsVec.data())) {
            continue;
        }

        if (RenderGlobals::width != lastWidth ||
            RenderGlobals::height != lastHeight) {
            image = Image::create(RenderGlobals::width, RenderGlobals::height);
            if (!image) continue;
        } else if (image) {
            image->clear();
        }

        renderImage(image.get());
        if (!image->saveToFile(
            PathUtil::appendSeqnum(filename, fileCounter))
            ) continue;

        lastWidth = RenderGlobals::width;
        lastHeight = RenderGlobals::height;
        fileCounter++;
    }
}

int main(int argc, char **argv) {
    if (ArgsParser::checkHelp(argc, argv)) return 0;

    MpfrGlobals::initMpfr();

    if (argsCount(argc) == 1 &&
        strcmp(argv[1], replOption) == 0) {
        return runRepl(argc, argv);
    } else {
        return runOnce(argc, argv);
    }
}