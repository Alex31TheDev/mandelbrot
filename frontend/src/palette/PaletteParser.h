#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "BackendAPI.h"

class PaletteParser {
public:
    using KeyValueMap = std::unordered_map<std::string, std::string>;

    PaletteParser(const std::string &skipOption);

    bool parse(const std::vector<std::string> &args,
        Backend::PaletteHexConfig &out, std::string &err
    ) const;

private:
    static constexpr char _commentToken = '#';
    std::string _skipOption;

    float _parseColorValue(const std::string &str,
        float defaultValue
    ) const;

    bool _isValidEntry(const Backend::PaletteHexEntry &entry) const;
    bool _validateConfig(const Backend::PaletteHexConfig &out,
        const std::string &context, std::string &err) const;
    bool _validateEntryCount(const Backend::PaletteHexConfig &out,
        const std::string &context, std::string &err) const;

    bool _parseCLIEntry(const std::string &str,
        Backend::PaletteHexEntry &out) const;
    bool _parseCLI(const std::vector<std::string> &args,
        Backend::PaletteHexConfig &out, std::string &err) const;

    bool _parseKeyValue(const std::string &str,
        std::string &key, std::string &value) const;
    bool _parseFileLine(const std::string &line,
        KeyValueMap &values, std::string &err) const;
    void _parseFileConfig(const KeyValueMap &values,
        Backend::PaletteHexConfig &out) const;
    bool _parseFileEntry(const KeyValueMap &values,
        Backend::PaletteHexEntry &entry, std::string &err) const;
    bool _parseFile(const std::string &filePath,
        Backend::PaletteHexConfig &out, std::string &err) const;
};
