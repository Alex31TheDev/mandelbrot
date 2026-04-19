#pragma once

#include <fstream>

template<typename Derived>
bool KeyValueWriter<Derived>::writeKeyValue(
    const std::string &filePath,
    std::string &err
) const {
    err.clear();

    std::ofstream fout(filePath);
    if (!fout.is_open()) {
        err = Derived::_openFileError;
        return false;
    }

    if (!Derived::_headerLine.empty()) {
        fout << _commentToken << ' ' << Derived::_headerLine << '\n';
    }

    _writeBody(fout);

    if (!fout.good()) {
        err = Derived::_writeFileError;
        return false;
    }

    return true;
}
