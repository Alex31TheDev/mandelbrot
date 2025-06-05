#include "args/ArgsParser.h"

#include "scalar/ScalarGlobals.h"
#include "mpfr/MpfrGlobals.h"
using namespace ScalarGlobals;

#include "image/Image.h"
#include "render/RenderImage.h"

const char filename[] = "mandelbrot.png";

int main(int argc, char **argv) {
    MpfrGlobals::initMpfr();

    if (!ArgsParser::parse(argc, argv)) return 1;

    auto image = Image::create(width, height);
    if (image == nullptr) return 1;

    if (useThreads) {
        renderImageParallel(image.get());
    } else {
        renderImage(image.get());
    }

    if (!image->saveToFile(filename)) return 1;
    return 0;
}