#if true

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
#include "util/fnv1a.h"
using namespace fnv1a;

#include "image/Image.h"
#include "render/RenderImage.h"

#ifndef OUT_FILENAME
#define OUT_FILENAME "mandelbrot"
#endif
#ifndef OUT_FILETYPE
#define OUT_FILETYPE "png"
#endif

static_assert(hash_32(OUT_FILETYPE) == "png"_hash_32 ||
    hash_32(OUT_FILETYPE) == "jpg"_hash_32 ||
    hash_32(OUT_FILETYPE) == "bmp"_hash_32,
    "OUT_FILETYPE must be one of: \"png\", \"jpg\", \"bmp\"");

static const char *fullname = OUT_FILENAME "." OUT_FILETYPE;

static bool initializeImage(std::unique_ptr<Image> &image) {
    image = Image::create(RenderGlobals::width, RenderGlobals::height);
    return image != nullptr;
}

static bool saveImage(const Image *image,
    const std::string &filename, int num = -1) {
    const bool appendDate = num < 0;

    const std::string outName = appendDate
        ? filename
        : PathUtil::appendSeqnum(filename, num);

    return image->saveToFile(outName, appendDate, OUT_FILETYPE);
}

static int runOnce(int argc, char **argv) {
    if (!ArgsParser::parse(argc, argv)) return 1;

    std::unique_ptr<Image> image;
    if (!initializeImage(image)) return 1;

    renderImage(image.get());

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
        } else if (image) {
            image->clear();
        }

        renderImage(image.get());

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

#else

#include <vector>
#include <cstdint>
#include "image/stb_image_write.h"
#include "scalar/ScalarColorPalette.h"
#include "vector/VectorRenderer.h"
#include "vector/VectorColorPalette.h"

int main() {
    std::vector<ScalarColor> entries = {
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
    };

    ScalarColorPalette palette(entries, 1.0f, true);
    VectorColorPalette vecp(palette);

    const int width = 8 * 64;
    const int height = 64;
    const int channels = 3;
    std::vector<uint8_t> image(width * height * channels + 100);

    for (int x = 0; x < width; x += 1) {
        float fx = (static_cast<float>(x) / width) * 1.0f + 0.5f;
        ScalarColor c = palette.sample(fx);

        //for (int y = 0; y < height; ++y) {
        //    int idx = (y * width + x) * channels;
        //    image[idx + 0] = static_cast<uint8_t>(c.R * 255.0f);
        //    image[idx + 1] = static_cast<uint8_t>(c.G * 255.0f);
        //    image[idx + 2] = static_cast<uint8_t>(c.B * 255.0f);
        //}

        alignas(SIMD_HALF_ALIGNMENT) scalar_half_t a[SIMD_HALF_WIDTH] = { 0 };
        for (int i = 0; i < SIMD_HALF_WIDTH; i++) {
            a[i] = (static_cast<scalar_half_t>(x + i) / width) * 1.0f + 0.5f;
        }
        simd_half_t val = SIMD_LOAD_H(a);
        simd_half_t R, G, B;
        vecp.sampleSIMD(val, R, G, B);
        for (int y = 0; y < height; ++y) {
            size_t idx = (y * width + x) * channels;
            VectorRenderer::setPixels_vec(image.data(), idx, SIMD_HALF_WIDTH, R, G, B);
        }
    }

    const char *filename = "palette_demo.png";
    if (!stbi_write_png(filename, width, height, channels, image.data(), width * channels)) {
        return 1;
    }
    return 0;
}

#endif