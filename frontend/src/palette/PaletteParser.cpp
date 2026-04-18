#include "PaletteParser.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <functional>
#include <vector>

#include "BackendAPI.h"
#include "util/ParserUtil.h"

using namespace ParserUtil;

PaletteParser::PaletteParser(const std::string &skipOption)
    : _skipOption(skipOption) {}

float PaletteParser::_parseColorValue(
    const std::string &str,
    float defaultValue
) const {
    if (str == _skipOption) return defaultValue;
    return parseNumber<float>(str, defaultValue);
}

bool PaletteParser::_isValidEntry(const Backend::PaletteHexEntry &entry) const {
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
        out.totalLength = _parseColorValue(args[0], out.totalLength);
    }

    if (args.size() > 1) {
        out.offset = _parseColorValue(args[1], out.offset);
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

bool PaletteParser::_parseKeyValue(
    const std::string &str,
    std::string &key, std::string &value
) const {
    const size_t split = str.find('=');
    if (split == std::string::npos) return false;

    key = std::string(str.substr(0, split));
    value = std::string(str.substr(split + 1));
    return !key.empty();
}

bool PaletteParser::_parseFileLine(
    const std::string &line,
    KeyValueMap &values, std::string &err
) const {
    std::istringstream iss{ line };
    std::string token;

    while (iss >> token) {
        std::string key;
        std::string value;
        if (!_parseKeyValue(token, key, value)) {
            err = "Invalid palette file token: " + token;
            return false;
        }

        values.insert_or_assign(std::move(key), std::move(value));
    }

    return true;
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
) const {
    out = {};
    err.clear();

    std::ifstream fin(filePath);
    if (!fin.is_open()) {
        err = "Failed to open palette file.";
        return false;
    }

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == _commentToken) {
            continue;
        }

        KeyValueMap values;
        if (!_parseFileLine(line, values, err)) return false;

        _parseFileConfig(values, out);

        err.clear();
        Backend::PaletteHexEntry entry;

        if (_parseFileEntry(values, entry, err)) {
            out.entries.push_back(entry);
        } else if (!err.empty()) return false;
    }

    if (!_validateConfig(out, "Palette file", err)) return false;
    return _validateEntryCount(out, "Palette file", err);
}

bool PaletteParser::parse(
    const std::vector<std::string> &args,
    Backend::PaletteHexConfig &out, std::string &err
) const {
    if (args.size() == 1 && args[0] != _skipOption) {
        return _parseFile(args[0], out, err);
    }

    return _parseCLI(args, out, err);
}