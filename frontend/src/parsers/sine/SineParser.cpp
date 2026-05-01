#include "SineParser.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "BackendAPI.h"
using namespace Backend;

#include "util/ParserUtil.h"

using namespace ParserUtil;

SineParser::SineParser(const std::string &skipOption)
    : _skipOption(skipOption) {}

bool SineParser::_validate(
    const SinePaletteConfig &out,
    const std::string &context, std::string &err
) const {
    if (std::abs(out.freqMult) > 0.0001f) return true;

    err = context + " frequency multiplier must be non-zero.";
    return false;
}

bool SineParser::_parseCLI(
    const std::vector<std::string> &args,
    SinePaletteConfig &out, std::string &err
) const {
    out = {};
    err.clear();

    if (args.size() > 4) {
        err = "Sine color accepts [freqR] [freqG] [freqB] [freqMult] "
            "or a sine file path.";
        return false;
    }

    if (!args.empty()) {
        out.freqR = parseValue(args[0], _skipOption, out.freqR);
    }

    if (args.size() > 1) {
        out.freqG = parseValue(args[1], _skipOption, out.freqG);
    }

    if (args.size() > 2) {
        out.freqB = parseValue(args[2], _skipOption, out.freqB);
    }

    if (args.size() > 3) {
        out.freqMult = parseValue(args[3], _skipOption, out.freqMult);
    }

    return _validate(out, "Sine color", err);
}

void SineParser::_parseFileConfig(
    const KeyValueMap &values,
    SinePaletteConfig &out
) const {
    if (const auto it = values.find("freqR"); it != values.end()) {
        out.freqR = parseNumber<float>(it->second, out.freqR);
    }

    if (const auto it = values.find("freqG"); it != values.end()) {
        out.freqG = parseNumber<float>(it->second, out.freqG);
    }

    if (const auto it = values.find("freqB"); it != values.end()) {
        out.freqB = parseNumber<float>(it->second, out.freqB);
    }

    if (const auto it = values.find("freqMult"); it != values.end()) {
        out.freqMult = parseNumber<float>(it->second, out.freqMult);
    }
}

bool SineParser::_parseFile(
    const std::string &filePath,
    SinePaletteConfig &out, std::string &err
) {
    const bool parsed = parseKeyValue(filePath, out, err);
    if (!parsed) {
        return false;
    }

    return _validate(out, "Sine file", err);
}

bool SineParser::_handleFileValues(
    const KeyValueMap &values,
    SinePaletteConfig &out,
    std::string &
) {
    _parseFileConfig(values, out);
    return true;
}

bool SineParser::parse(
    const std::vector<std::string> &args,
    SinePaletteConfig &out, std::string &err
) {
    if (args.size() == 1 && args[0] != _skipOption) {
        return _parseFile(args[0], out, err);
    }

    return _parseCLI(args, out, err);
}