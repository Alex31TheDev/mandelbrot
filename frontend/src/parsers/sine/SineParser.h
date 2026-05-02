#pragma once
#include "parsers/KeyValueParser.h"

#include <string>
#include <vector>

#include "BackendAPI.h"

class SineParser
    : public KeyValueParser<Backend::SinePaletteConfig> {
public:
    explicit SineParser(const std::string &skipOption);

    bool parse(const std::vector<std::string> &args,
        Backend::SinePaletteConfig &out, std::string &err);

private:
    std::string _skipOption;

    static bool _validate(const Backend::SinePaletteConfig &out,
        const std::string &context, std::string &err);

    bool _parseCLI(const std::vector<std::string> &args,
        Backend::SinePaletteConfig &out, std::string &err) const;
    void _parseFileConfig(const KeyValueMap &values,
        Backend::SinePaletteConfig &out) const;
    bool _parseFile(const std::string &filePath,
        Backend::SinePaletteConfig &out, std::string &err);

    bool _handleFileValues(const KeyValueMap &values,
        Backend::SinePaletteConfig &out, std::string &err) override;

    std::string _openFileError() const override {
        return "Failed to open sine file.";
    }
    std::string _invalidTokenErrorPrefix() const override {
        return "Invalid sine file token: ";
    }
};
