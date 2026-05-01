#pragma once
#include "../KeyValueParser.h"

#include <string>
#include <vector>

#include "BackendAPI.h"

class PaletteParser
    : public KeyValueParser<Backend::PaletteHexConfig> {
public:
    PaletteParser(const std::string &skipOption);

    bool parse(const std::vector<std::string> &args,
        Backend::PaletteHexConfig &out, std::string &err);

private:
    std::string _skipOption;

    bool _isValidEntry(const Backend::PaletteHexEntry &entry) const;
    bool _validateConfig(const Backend::PaletteHexConfig &out,
        const std::string &context, std::string &err) const;
    bool _validateEntryCount(const Backend::PaletteHexConfig &out,
        const std::string &context, std::string &err) const;

    bool _parseCLIEntry(const std::string &str,
        Backend::PaletteHexEntry &out) const;
    bool _parseCLI(const std::vector<std::string> &args,
        Backend::PaletteHexConfig &out, std::string &err) const;

    void _parseFileConfig(const KeyValueMap &values,
        Backend::PaletteHexConfig &out) const;
    bool _parseFileEntry(const KeyValueMap &values,
        Backend::PaletteHexEntry &entry, std::string &err) const;
    bool _parseFile(const std::string &filePath,
        Backend::PaletteHexConfig &out, std::string &err);

    bool _handleFileValues(const KeyValueMap &values,
        Backend::PaletteHexConfig &out, std::string &err) override;

    std::string _openFileError() const override {
        return "Failed to open palette file.";
    }
    std::string _invalidTokenErrorPrefix() const override {
        return "Invalid palette file token: ";
    }
};
