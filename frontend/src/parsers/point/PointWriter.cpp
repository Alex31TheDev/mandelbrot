#include "PointWriter.h"

#include <ostream>

PointWriter::PointWriter(const PointConfig &point)
    : _point(point) {}

bool PointWriter::write(const std::string &filePath, std::string &err) const {
    return writeKeyValue(filePath, err);
}

void PointWriter::_writeBody(std::ostream &out) const {
    out << "real=" << _point.real << '\n'
        << "imag=" << _point.imag << '\n'
        << "zoom=" << _point.zoom << '\n';
}

