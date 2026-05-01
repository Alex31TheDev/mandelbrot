#pragma once

#include <string>

#include "parsers/KeyValueParser.h"

struct PointConfig {
    std::string fractal = "mandelbrot";
    bool inverse = false;
    bool julia = false;
    int iterations = 0;
    std::string real;
    std::string imag;
    std::string zoom = "0";
    std::string seedReal = "0";
    std::string seedImag = "0";
};

class PointParser : public KeyValueParser<PointConfig> {
public:
    bool parse(const std::string &filePath, PointConfig &out, std::string &err);

private:
    bool _handleFileValues(const KeyValueMap &values,
        PointConfig &out, std::string &err) override;

    std::string _openFileError() const override {
        return "Failed to open point file.";
    }
    std::string _invalidTokenErrorPrefix() const override {
        return "Invalid point file token: ";
    }
};
