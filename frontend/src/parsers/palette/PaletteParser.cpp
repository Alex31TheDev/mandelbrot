#include "PaletteParser.h"

#include <algorithm>
#include <string>
#include <utility>
#include <functional>
#include <vector>

#include "BackendAPI.h"

#include "util/ParserUtil.h"

using namespace ParserUtil;

PaletteParser::PaletteParser(const std::string &skipOption)
    : _skipOption(skipOption) {}

bool PaletteParser::_isValidEntry(
    const Backend::PaletteHexEntry &entry
) const {
    bool ok = false;
    parseHexString(entry.color, std::ref(ok));
    return ok && entry.length >= 0.0f;
}

bool PaletteParser::_validateConfig(
    const Backend::PaletteHexConfig &out,
    const std::string &context, std::string &err
) const {
    if (out.totalLength > 0.0f && out.offset >= 0.0f) return true;

    err = context + " length must be > 0 and offset must be >= 0.";
    return false;
}

bool PaletteParser::_validateEntryCount(
    const Backend::PaletteHexConfig &out,
    const std::string &context, std::string &err
) const {
    if (out.entries.size() >= 2) return true;

    err = context + " must contain at least 2 color entries.";
    return false;
}

bool PaletteParser::_parseCLIEntry(
    const std::string &str,
    Backend::PaletteHexEntry &out
) const {
    const size_t split = str.find(':');

    std::string hex = str.substr(0, split);
    hex.erase(
        std::remove_if(hex.begin(), hex.end(),
            [](const char c) { return c == '\'' || c == '"'; }),
        hex.end()
    );

    const std::string length =
        split == std::string::npos ? "1"
        : str.substr(split + 1);

    out = {
        .color = hex,
        .length = parseNumber<float>(length, 1.0f)
    };

    return _isValidEntry(out);
}

bool PaletteParser::_parseCLI(
    const std::vector<std::string> &args,
    Backend::PaletteHexConfig &out, std::string &err
) const {
    out = {};
    err.clear();

    if (!args.empty()) {
        out.totalLength = parseValue(args[0], _skipOption, out.totalLength);
    }

    if (args.size() > 1) {
        out.offset = parseValue(args[1], _skipOption, out.offset);
    }

    if (!_validateConfig(out, "Palette", err)) return false;

    for (size_t i = 2; i < args.size(); ++i) {
        const std::string &arg = args[i];
        if (arg == _skipOption) continue;

        Backend::PaletteHexEntry entry;
        if (!_parseCLIEntry(arg, entry)) {
            err = "Palette entries must use #RRGGBB or #RRGGBB:length.";
            return false;
        }

        out.entries.push_back(entry);
    }

    return _validateEntryCount(out, "Palette", err);
}

void PaletteParser::_parseFileConfig(
    const KeyValueMap &values,
    Backend::PaletteHexConfig &out
) const {
    if (const auto it = values.find("totalLength"); it != values.end()) {
        out.totalLength = parseNumber<float>(it->second, out.totalLength);
    }

    if (const auto it = values.find("offset"); it != values.end()) {
        out.offset = parseNumber<float>(it->second, out.offset);
    }

    if (const auto it = values.find("blendEnds"); it != values.end()) {
        bool ok = false;
        const bool parsed = parseBool(it->second, std::ref(ok));
        if (ok) out.blendEnds = parsed;
    }
}

bool PaletteParser::_parseFileEntry(
    const KeyValueMap &values,
    Backend::PaletteHexEntry &entry, std::string &err
) const {
    const auto colorIt = values.find("color");
    if (colorIt == values.end()) return false;

    const auto lengthIt = values.find("length");
    const std::string length =
        lengthIt == values.end() ? "1" : lengthIt->second;

    entry = {
        .color = colorIt->second,
        .length = parseNumber<float>(length, 1.0f)
    };
    if (_isValidEntry(entry)) return true;

    err = "Invalid palette file color entry.";
    return false;
}

bool PaletteParser::_parseFile(
    const std::string &filePath,
    Backend::PaletteHexConfig &out, std::string &err
) {
    const bool parsed = parseKeyValue(filePath, out, err);
    if (!parsed) {
        return false;
    }

    if (!_validateConfig(out, "Palette file", err)) return false;
    return _validateEntryCount(out, "Palette file", err);
}

bool PaletteParser::_handleFileValues(
    const KeyValueMap &values,
    Backend::PaletteHexConfig &out,
    std::string &err
) {
    _parseFileConfig(values, out);

    err.clear();
    Backend::PaletteHexEntry entry;
    if (_parseFileEntry(values, entry, err)) {
        out.entries.push_back(entry);
    }

    return err.empty();
}

bool PaletteParser::parse(
    const std::vector<std::string> &args,
    Backend::PaletteHexConfig &out, std::string &err
) {
    if (args.size() == 1 && args[0] != _skipOption) {
        return _parseFile(args[0], out, err);
    }

    return _parseCLI(args, out, err);
}