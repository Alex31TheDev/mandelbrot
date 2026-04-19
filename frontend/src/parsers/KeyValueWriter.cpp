#include "KeyValueWriter.h"

#include <fstream>

bool KeyValueWriter::writeKeyValue(
    const std::string &filePath,
    std::string &err
) const {
    err.clear();

    std::ofstream fout(filePath);
    if (!fout.is_open()) {
        err = _openFileError();
        return false;
    }

    const std::string header = _headerLine();
    if (!header.empty()) {
        fout << _commentToken << ' ' << header << '\n';
    }

    _writeBody(fout);

    if (!fout.good()) {
        err = _writeFileError();
        return false;
    }

    return true;
}

