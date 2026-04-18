#pragma once

#include <string>

struct CPUInfo {
    std::string name = "Unknown";
    int cores = -1;
    int threads = -1;
};

CPUInfo queryCPUInfo();
