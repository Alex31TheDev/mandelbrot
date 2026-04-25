#include "PointWriter.h"

#include <ostream>

#include "util/FormatUtil.h"

PointWriter::PointWriter(const PointConfig &point)
    : _point(point) {}

bool PointWriter::write(const std::string &filePath, std::string &err) const {
    return writeKeyValue(filePath, err);
}

void PointWriter::_writeBody(std::ostream &out) const {
    out << "fractal=" << _point.fractal << '\n'
        << "inverse=" << FormatUtil::formatBool(_point.inverse) << '\n'
        << "julia=" << FormatUtil::formatBool(_point.julia) << '\n'
        << "iterations=" << _point.iterations << '\n'
        << "real=" << _point.real << '\n'
        << "imag=" << _point.imag << '\n'
        << "zoom=" << _point.zoom << '\n'
        << "seed_real=" << _point.seedReal << '\n'
        << "seed_imag=" << _point.seedImag << '\n';
}