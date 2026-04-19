#pragma once

#include <iosfwd>
#include <string>

class KeyValueWriter {
public:
    virtual ~KeyValueWriter() = default;

protected:
    bool writeKeyValue(const std::string &filePath, std::string &err) const;

    virtual void _writeBody(std::ostream &out) const = 0;

    virtual std::string _headerLine() const { return ""; }
    virtual std::string _openFileError() const = 0;
    virtual std::string _writeFileError() const = 0;

private:
    static constexpr char _commentToken = '#';
};
