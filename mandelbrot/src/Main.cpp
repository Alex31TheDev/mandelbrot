#include "args/ArgsParser.h"
#include "scalar/ScalarGlobals.h"

#include "image/Image.h"
#include "render/RenderImage.h"

const char filename[] = "mandelbrot.png";

int main(int argc, char **argv) {
    if (!ArgsParser::parse(argc, argv)) return 1;

    auto image = Image::create(ScalarGlobals::width, ScalarGlobals::height);
    if (image == nullptr) return 1;

    if (ScalarGlobals::useThreads) {
        renderImageParallel(*image);
    } else {
        renderImage(*image);
    }

    if (!image->saveToFile(filename)) return 1;
    return 0;
}