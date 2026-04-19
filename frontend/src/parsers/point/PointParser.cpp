#include "PointParser.h"

#include <cmath>
#include <functional>
#include <limits>

#include "util/ParserUtil.h"

using namespace ParserUtil;

bool PointParser::parse(
    const std::string &filePath,
    PointConfig &out,
    std::string &err
) {
    const bool parsed = parseKeyValue(filePath, out, err);
    if (!parsed) {
        return false;
    }

    if (out.real.empty() || out.imag.empty()) {
        err = "Point file must contain real and imag values.";
        return false;
    }

    if (!std::isfinite(out.zoom)) {
        err = "Point file zoom must be a finite number.";
        return false;
    }

    return true;
}

bool PointParser::_handleFileValues(
    const KeyValueMap &values,
    PointConfig &out,
    std::string &
) {
    if (const auto it = values.find("real"); it != values.end()) {
        out.real = it->second;
    }

    if (const auto it = values.find("imag"); it != values.end()) {
        out.imag = it->second;
    }

    if (const auto it = values.find("zoom"); it != values.end()) {
        bool ok = false;
        out.zoom = parseNumber<double>(it->second, std::ref(ok), out.zoom);
        if (!ok) {
            out.zoom = std::numeric_limits<double>::quiet_NaN();
        }
    }

    return true;
}
