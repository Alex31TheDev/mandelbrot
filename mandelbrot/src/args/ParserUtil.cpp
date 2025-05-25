#include "ParserUtil.h"

#include <cctype>
#include <cstring>

namespace ParserUtil {
    bool parseBool(const char *str, bool &ok) {
        if (!str) {
            ok = false;
            return false;
        }

        size_t len = strlen(str);

        if (len != 4 && len != 5) {
            ok = false;
            return false;
        }

        char lower[6] = { 0 };

        for (int i = 0; i < 5 && str[i]; i++) {
            lower[i] = static_cast<char>(tolower(str[i]));
        }

        if (strcmp(lower, "true") == 0) {
            ok = true;
            return true;
        } else if (strcmp(lower, "false") == 0) {
            ok = true;
            return false;
        }

        ok = false;
        return false;
    }
}