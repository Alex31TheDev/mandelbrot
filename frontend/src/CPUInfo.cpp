#include "CPUInfo.h"

#include "util/StringUtil.h"

#include <libcpuid/libcpuid.h>

CPUInfo queryCPUInfo() {
    CPUInfo info{};

    cpu_id_t id = {};
    if (cpu_identify(nullptr, &id) < 0) {
        return info;
    }

    if (id.brand_str[0] != '\0' &&
        StringUtil::toLower(id.cpu_codename) != "unknown") {
        info.name = id.brand_str;
    }

    info.cores = id.num_cores;
    info.threads = id.num_logical_cpus;
    return info;
}
