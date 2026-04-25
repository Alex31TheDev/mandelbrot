#pragma once

#include <fstream>
#include <sstream>
#include <utility>

#include "util/ParserUtil.h"

template<typename Output>
bool KeyValueParser<Output>::_parseTokenKeyValue(
    const std::string &str,
    std::string &key, std::string &value
) const {
    const size_t split = str.find('=');
    if (split == std::string::npos) return false;

    key = std::string(str.substr(0, split));
    value = std::string(str.substr(split + 1));
    return !key.empty();
}

template<typename Output>
bool KeyValueParser<Output>::_parseFileLine(
    const std::string &line,
    KeyValueMap &values, std::string &err
) const {
    std::istringstream iss{ line };
    std::string token;

    while (iss >> token) {
        std::string key, value;
        if (!_parseTokenKeyValue(token, key, value)) {
            err = _invalidTokenErrorPrefix() + token;
            return false;
        }

        values.insert_or_assign(std::move(key), std::move(value));
    }

    return true;
}

template<typename Output>
float KeyValueParser<Output>::parseValue(
    const std::string &str,
    const std::string &skipOption,
    float defaultValue
) const {
    if (str == skipOption) return defaultValue;
    return ParserUtil::parseNumber<float>(str, defaultValue);
}

template<typename Output>
bool KeyValueParser<Output>::parseKeyValue(
    const std::string &filePath,
    Output &out,
    std::string &err
) {
    err.clear();
    out = {};

    std::ifstream fin(filePath);
    if (!fin.is_open()) {
        err = _openFileError();
        return false;
    }

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == _commentToken) {
            continue;
        }

        KeyValueMap values;
        if (!_parseFileLine(line, values, err)) {
            return false;
        }

        if (!_handleFileValues(values, out, err)) {
            return false;
        }
    }

    return true;
}
