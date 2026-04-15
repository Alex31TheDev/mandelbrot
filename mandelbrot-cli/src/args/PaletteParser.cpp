#include "PaletteParser.h"
#include "ArgsParser.h"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "BackendApi.h"
#include "util/ParserUtil.h"

#include "ArgsUsage.h"
using namespace ArgsUsage;
using namespace ParserUtil;

using KeyValueMap = std::unordered_map<std::string, std::string>;

static bool isValidEntry(const Backend::PaletteEntry &entry) {
    bool ok = false;
    parseHexString(entry.color, std::ref(ok));
    return ok && entry.length >= 0.0f;
}

static bool validateConfig(
    const Backend::PaletteConfig &out,
    const std::string &context, std::string &err
) {
    if (out.totalLength > 0.0f && out.offset >= 0.0f) return true;

    err = context + " length must be > 0 and offset must be >= 0.";
    return false;
}

static bool validateEntryCount(
    const Backend::PaletteConfig &out,
    const std::string &context, std::string &err
) {
    if (out.entries.size() >= 2) return true;

    err = context + " must contain at least 2 color entries.";
    return false;
}

static bool parseCLIEntry(
    const std::string &str,
    Backend::PaletteEntry &out
) {
    const size_t split = str.find(':');

    const std::string hex = str.substr(0, split);
    const std::string length =
        split == std::string::npos ? "1"
        : str.substr(split + 1);

    out = {
        .color = hex,
        .length = parseNumber<float>(length, 1.0f)
    };
    return isValidEntry(out);
}

static bool parseCLI(
    const std::vector<std::string> &args,
    Backend::PaletteConfig &out, std::string &err
) {
    out = {};
    err.clear();

    if (args.size() > 0) {
        out.totalLength = ArgsParser::parseColorValue(args[0], out.totalLength);
    }

    if (args.size() > 1) {
        out.offset = ArgsParser::parseColorValue(args[1], out.offset);
    }

    if (!validateConfig(out, "Palette", err)) return false;

    for (size_t i = 2; i < args.size(); ++i) {
        const std::string &arg = args[i];
        if (arg == skipOption) continue;

        Backend::PaletteEntry entry;
        if (!parseCLIEntry(arg, entry)) {
            err = "Palette entries must use #RRGGBB or #RRGGBB:length.";
            return false;
        }

        out.entries.push_back(entry);
    }

    return validateEntryCount(out, "Palette", err);
}

static bool parseKeyValue(
    const std::string &str,
    std::string &key, std::string &value
) {
    const size_t split = str.find('=');
    if (split == std::string::npos) return false;

    key = std::string(str.substr(0, split));
    value = std::string(str.substr(split + 1));
    return !key.empty();
}

static bool parseFileLine(
    const std::string &line,
    KeyValueMap &values,
    std::string &err
) {
    std::istringstream iss{ line };
    std::string token;

    while (iss >> token) {
        std::string key, value;
        if (!parseKeyValue(token, key, value)) {
            err = "Invalid palette file token: " + token;
            return false;
        }

        values.insert_or_assign(std::move(key), std::move(value));
    }

    return true;
}

static void parseFileConfig(const KeyValueMap &values, Backend::PaletteConfig &out) {
    if (const auto it = values.find("totalLength"); it != values.end()) {
        out.totalLength = parseNumber<float>(it->second, out.totalLength);
    }

    if (const auto it = values.find("offset"); it != values.end()) {
        out.offset = parseNumber<float>(it->second, out.offset);
    }
}

static bool parseFileEntry(
    const KeyValueMap &values,
    Backend::PaletteEntry &entry, std::string &err
) {
    const auto colorIt = values.find("color");
    if (colorIt == values.end()) return false;

    const auto lengthIt = values.find("length");
    const std::string length =
        lengthIt == values.end() ? "1" : lengthIt->second;

    entry = {
        .color = colorIt->second,
        .length = parseNumber<float>(length, 1.0f)
    };
    if (isValidEntry(entry)) return true;

    err = "Invalid palette file color entry.";
    return false;
}

static bool parseFile(
    const std::string &filePath,
    Backend::PaletteConfig &out, std::string &err
) {
    out = {};
    err.clear();

    std::ifstream fin(filePath);
    if (!fin.is_open()) {
        err = "Failed to open palette file.";
        return false;
    }

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == PaletteParser::commentToken) {
            continue;
        }

        KeyValueMap values;
        if (!parseFileLine(line, values, err)) return false;

        parseFileConfig(values, out);

        err.clear();
        Backend::PaletteEntry entry;

        if (parseFileEntry(values, entry, err)) {
            out.entries.push_back(entry);
        } else if (!err.empty()) return false;
    }

    if (!validateConfig(out, "Palette file", err)) return false;
    return validateEntryCount(out, "Palette file", err);
}

bool PaletteParser::parse(
    const std::vector<std::string> &args,
    Backend::PaletteConfig &out, std::string &err
) {
    if (args.size() == 1 && args[0] != skipOption) {
        return parseFile(args[0], out, err);
    } else {
        return parseCLI(args, out, err);
    }
}
