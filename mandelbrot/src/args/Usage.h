#pragma once

#include "../util/RangeUtil.h"

#define MIN_ARGS 5
#define MAX_ARGS 17

extern const char *usage;

DECLARE_RANGE_ARRAY(char *, helpOptions);

extern const char *replOption;
extern const char *exitOption;

[[nodiscard]] inline int argsCount(int argc) { return argc - 1; };
void printUsage(const char *progPath = nullptr, bool error = false);
