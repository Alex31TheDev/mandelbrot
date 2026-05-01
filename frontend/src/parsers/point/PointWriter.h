#pragma once

#include <string>
#include <ostream>

#include "PointParser.h"
#include "../KeyValueWriter.h"

class PointWriter : public KeyValueWriter {
public:
    explicit PointWriter(const PointConfig &point);

    bool write(const std::string &filePath, std::string &err) const;

private:
    PointConfig _point;

    void _writeBody(std::ostream &out) const override;

    std::string _headerLine() const override {
        return "Point file";
    }
    std::string _openFileError() const override {
        return "Failed to open point file for writing.";
    }
    std::string _writeFileError() const override {
        return "Failed while writing point file.";
    }
};
