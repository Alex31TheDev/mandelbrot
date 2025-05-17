#pragma once

namespace ArgsParser {
    constexpr int MIN_ARGS = 6;
    constexpr int MAX_ARGS = 11;

    bool parse(int argc, char **argv);
}
