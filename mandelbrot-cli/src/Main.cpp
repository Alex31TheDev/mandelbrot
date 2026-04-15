#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libcpuid/libcpuid.h>

#include "args/ArgsParser.h"
#include "args/ArgsUsage.h"

#include "BackendModule.h"
#include "CallbackFormatter.h"
#include "util/ParserUtil.h"
#include "util/PathUtil.h"

const char OUT_FILENAME[] = "mandelbrot";
const char OUT_FILETYPE[] = "png";

static std::string outputFilename() {
    return std::string(OUT_FILENAME) + "." + OUT_FILETYPE;
}

static bool saveImage(Backend::Session &session,
    std::optional<int> num = std::nullopt) {
    const std::string baseName = outputFilename();
    const std::string outName = num
        ? PathUtil::appendSeqnum(baseName, *num)
        : baseName;

    const Backend::Status status = session.saveImage(outName, !num, OUT_FILETYPE);
    if (!status) {
        fprintf(stderr, "%s\n", status.message.c_str());
        return false;
    }

    return true;
}

static std::string getCpuName() {
    cpu_raw_data_t raw = {};
    if (cpuid_get_raw_data(&raw) < 0) {
        return cpuid_error();
    }

    cpu_id_t id = {};
    if (cpu_identify(&raw, &id) < 0) {
        return cpuid_error();
    }

    return id.brand_str;
}

static bool runOnce(Backend::Session &session, int argc, char **argv) {
    const Backend::Status parseStatus = ArgsParser::parse(session, argc, argv);
    if (!parseStatus) {
        fprintf(stderr, "%s\n", parseStatus.message.c_str());
        return false;
    }

    const Backend::Status renderStatus = session.render();
    if (!renderStatus) {
        fprintf(stderr, "%s\n", renderStatus.message.c_str());
        return false;
    }

    return saveImage(session, 1);
}

static int runRepl(Backend::Session &session, char *argv0) {
    int fileCounter = 1;

    while (true) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        ArgsVec parsedArgs = ArgsVec::fromParsed(argv0,
            ParserUtil::parseCommandLine(line));

        if (ArgsUsage::argsCount(parsedArgs.argc) == 1 &&
            std::string_view(parsedArgs.argv[1]) == ArgsUsage::exitOption) {
            break;
        }

        if (ArgsParser::checkHelp(parsedArgs.argc, parsedArgs.argv)) {
            continue;
        }

        const Backend::Status parseStatus =
            ArgsParser::parse(session, parsedArgs.argc, parsedArgs.argv);
        if (!parseStatus) {
            fprintf(stderr, "%s\n", parseStatus.message.c_str());
            continue;
        }

        const Backend::Status renderStatus = session.render();
        if (!renderStatus) {
            fprintf(stderr, "%s\n", renderStatus.message.c_str());
            continue;
        }

        if (saveImage(session, fileCounter)) {
            ++fileCounter;
        }
    }

    return 0;
}

static std::vector<char *> makeForwardedArgs(int argc, char **argv) {
    std::vector<char *> forwardedArgs;
    forwardedArgs.reserve(static_cast<size_t>(argc));
    forwardedArgs.push_back(argv[0]);

    for (int i = 2; i < argc; ++i) {
        forwardedArgs.push_back(argv[i]);
    }

    return forwardedArgs;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        ArgsUsage::printTopLevelUsage(argv ? argv[0] : nullptr);
        return 1;
    }

    const std::string_view selector = argv[1] ? argv[1] : "";
    if (selector == "-h" || selector == "--help" || selector == "help") {
        ArgsUsage::printTopLevelUsage(argv ? argv[0] : nullptr);
        return 0;
    }

    const std::string configName = ArgsUsage::resolveVariant(selector);
    if (configName.empty()) {
        fprintf(stderr, "Unknown variant: %.*s\n\n",
            static_cast<int>(selector.size()), selector.data());
        ArgsUsage::printTopLevelUsage(argv ? argv[0] : nullptr);
        return 1;
    }

    std::string error;
    BackendModule backend = loadBackendModule(executableDir(), configName, error);
    if (!backend) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    CallbackFormatter formatter;
    formatter.bind(*backend.session);

    std::vector<char *> forwardedArgs = makeForwardedArgs(argc, argv);
    if (ArgsParser::checkHelp(static_cast<int>(forwardedArgs.size()),
        forwardedArgs.data())) {
        return 0;
    }

    printf("CPU: %s\n", getCpuName().c_str());

    if (ArgsUsage::argsCount(static_cast<int>(forwardedArgs.size())) == 1 &&
        std::string_view(forwardedArgs[1]) == ArgsUsage::replOption) {
        return runRepl(*backend.session, argv[0]);
    }

    return static_cast<int>(!runOnce(*backend.session,
        static_cast<int>(forwardedArgs.size()),
        forwardedArgs.data()));
}
