#pragma once

#include <iosfwd>
#include <string>

template<typename Derived>
class KeyValueWriter {
public:
    virtual ~KeyValueWriter() = default;

protected:
    bool writeKeyValue(const std::string &filePath, std::string &err) const;

    virtual void _writeBody(std::ostream &out) const = 0;

private:
    static constexpr char _commentToken = '#';
};

#include "KeyValueWriter_impl.h"
