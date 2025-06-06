#include "args/ArgsParser.h"

#include "render/RenderGlobals.h"
#include "mpfr/MpfrGlobals.h"

#include "image/Image.h"
#include "render/RenderImage.h"

const char filename[] = "mandelbrot.png";

int main(int argc, char **argv) {
    MpfrGlobals::initMpfr();

    if (!ArgsParser::parse(argc, argv)) return 1;

    auto image = Image::create(RenderGlobals::width, RenderGlobals::height);
    if (image == nullptr) return 1;

    renderImage(image.get());

    if (!image->saveToFile(filename)) return 1;
    return 0;
}