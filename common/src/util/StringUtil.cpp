#include "StringUtil.h"

#include <cctype>
#include <codecvt>
#include <locale>
#include <string>
#include <string_view>

#include "IncludeWin32.h"

namespace StringUtil {
    bool startsWith(std::string_view value, std::string_view prefix) {
        return value.size() >= prefix.size() &&
            value.compare(0, prefix.size(), prefix) == 0;
    }

    bool endsWith(std::string_view value, std::string_view suffix) {
        return value.size() >= suffix.size() &&
            value.substr(value.size() - suffix.size()) == suffix;
    }

    std::string toLower(std::string_view str) {
        std::string out(str.size(), '\0');

        for (size_t i = 0; i < str.size(); i++) {
            out[i] = static_cast<char>(
                tolower(static_cast<unsigned char>(str[i]))
            );
        }

        return out;
    }

    std::string toUpper(std::string_view str) {
        std::string out(str.size(), '\0');

        for (size_t i = 0; i < str.size(); i++) {
            out[i] = static_cast<char>(
                toupper(static_cast<unsigned char>(str[i]))
            );
        }

        return out;
    }

    std::string_view trimWhitespace(std::string_view value) {
        size_t begin = 0;
        while (begin < value.size() && (
            value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\n' || value[begin] == '\r' ||
            value[begin] == '\f' || value[begin] == '\v'
            )) begin++;

        size_t end = value.size();
        while (end > begin && (
            value[end - 1] == ' ' || value[end - 1] == '\t' ||
            value[end - 1] == '\n' || value[end - 1] == '\r' ||
            value[end - 1] == '\f' || value[end - 1] == '\v'
            )) end--;

        return value.substr(begin, end - begin);
    }

    std::string appendSuffix(
        std::string_view value,
        std::string_view suffix
    ) {
        if (suffix.empty()) return std::string(value);
        if (endsWith(value, suffix)) return std::string(value);

        std::string out;
        out.reserve(value.size() + suffix.size());
        out.append(value);
        out.append(suffix);
        return out;
    }

    std::string stripSuffix(
        std::string_view value,
        std::string_view suffix
    ) {
        if (suffix.empty()) return std::string(value);
        if (endsWith(value, suffix)) {
            return std::string(value.substr(0, value.size() - suffix.size()));
        }

        return std::string(value);
    }

    std::wstring utf8ToWide(std::string_view value) {
        if (value.empty()) return L"";

#ifdef _WIN32
        const size_t len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);

        if (len > 0) {
            std::wstring out(len, L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), out.data(), len);
            return out;
        }
#else
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.from_bytes(value.data(), value.data() + value.size());
        } catch (...) {
        }
#endif

        return std::wstring(value.begin(), value.end());
    }

#ifdef _WIN32
    void copyToClipboard(HWND hwnd, const std::wstring &text) {
        if (!OpenClipboard(hwnd)) return;
        EmptyClipboard();

        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);

        if (mem) {
            void *ptr = GlobalLock(mem);

            if (ptr) {
                memcpy(ptr, text.c_str(), bytes);
                GlobalUnlock(mem);

                if (SetClipboardData(CF_UNICODETEXT, mem)) {
                    mem = nullptr;
                }
            }

            if (mem) GlobalFree(mem);
        }

        CloseClipboard();
    }

    void copyToClipboard(HWND hwnd, std::string_view text) {
        copyToClipboard(hwnd, utf8ToWide(text));
    }
#endif
}