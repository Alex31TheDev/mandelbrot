#pragma once

#include <string>
#include <unordered_map>

template<typename Output>
class KeyValueParser {
protected:
    using KeyValueMap = std::unordered_map<std::string, std::string>;

    bool parseKeyValue(const std::string &filePath,
        Output &out, std::string &err);
    float parseValue(const std::string &str,
        const std::string &skipOption, float defaultValue) const;

    virtual bool _handleFileValues(const KeyValueMap &values,
        Output &out, std::string &err) = 0;

    virtual std::string _openFileError() const = 0;
    virtual std::string _invalidTokenErrorPrefix() const = 0;

private:
    static constexpr char _commentToken = '#';

    bool _parseTokenKeyValue(const std::string &str,
        std::string &key, std::string &value) const;
    bool _parseFileLine(const std::string &line,
        KeyValueMap &values, std::string &err) const;
};

#include "KeyValueParser_impl.h"
