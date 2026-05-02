#pragma once


#define MANDELBROT_GUI_VERSION_MAJOR 0
#define MANDELBROT_GUI_VERSION_MINOR 1
#define MANDELBROT_GUI_VERSION_PATCH 0

#define MANDELBROT_GUI_VERSION "0.1.0"

#ifndef RC_INVOKED

#include <string>

namespace GUI::Constants {
    inline const std::string appVersion = MANDELBROT_GUI_VERSION;
}

#endif
