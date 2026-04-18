#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "BackendApi.h"

class PaletteParser {
public:
    using KeyValueMap = std::unordered_map<std::string, std::string>;

    PaletteParser(const std::string &skipOption);

    bool parse(const std::vector<std::string> &args,
        Backend::PaletteConfig &out, std::string &err
    ) const;

private:
    static constexpr char _commentToken = '#';
    std::string _skipOption;

    float _parseColorValue(const std::string &str,
        float defaultValue
    ) const;

    bool _isValidEntry(const Backend::PaletteEntry &entry) const;
    bool _validateConfig(const Backend::PaletteConfig &out,
        const std::string &context, std::string &err) const;
    bool _validateEntryCount(const Backend::PaletteConfig &out,
        const std::string &context, std::string &err) const;

    bool _parseCLIEntry(const std::string &str,
        Backend::PaletteEntry &out) const;
    bool _parseCLI(const std::vector<std::string> &args,
        Backend::PaletteConfig &out, std::string &err) const;

    bool _parseKeyValue(const std::string &str,
        std::string &key, std::string &value) const;
    bool _parseFileLine(const std::string &line,
        KeyValueMap &values, std::string &err) const;
    void _parseFileConfig(const KeyValueMap &values,
        Backend::PaletteConfig &out) const;
    bool _parseFileEntry(const KeyValueMap &values,
        Backend::PaletteEntry &entry, std::string &err) const;
    bool _parseFile(const std::string &filePath,
        Backend::PaletteConfig &out, std::string &err) const;
};
