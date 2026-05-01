#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "BackendAPI.h"
#include "BackendModule.h"
using namespace Backend;

#include "args/ArgsParser.h"
#include "args/ArgsUsage.h"

#include "util/ParserUtil.h"
#include "util/PathUtil.h"

#include "CPUInfo.h"
#include "CallbackFormatter.h"
#include "fullname.h"

static bool saveImage(Session &session,
    std::optional<int> num = std::nullopt) {
    const std::string baseName = fullname;
    const std::string outName = num
        ? PathUtil::appendSeqnum(baseName, *num)
        : baseName;

    if (const Status status =
        session.saveImage(outName, !num, OUT_FILETYPE);
        !status) {
        fprintf(stderr, "%s\n", status.message.c_str());
        return false;
    }

    return true;
}

static bool runOnce(Session &session, int argc, char **argv) {
    if (const Status status = ArgsParser::parse(session, argc, argv);
        !status) {
        fprintf(stderr, "%s\n", status.message.c_str());
        return false;
    }

    if (const Status status = session.render(); !status) {
        fprintf(stderr, "%s\n", status.message.c_str());
        return false;
    }

    return saveImage(session, 1);
}

static int runRepl(Session &session, char *progName) {
    int fileCounter = 1;

    while (true) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        ArgsVec parsedArgs = ArgsVec::fromParsed(progName,
            ParserUtil::parseCommandLine(line));

        if (ArgsUsage::argsCount(parsedArgs.argc) == 1 &&
            std::string_view(parsedArgs.argv[1]) == ArgsUsage::exitOption) {
            break;
        }

        if (ArgsParser::checkHelp(parsedArgs.argc, parsedArgs.argv)) {
            continue;
        }

        if (const Status status =
            ArgsParser::parse(session, parsedArgs.argc, parsedArgs.argv);
            !status) {
            fprintf(stderr, "%s\n", status.message.c_str());
            continue;
        }

        if (const Status status = session.render(); !status) {
            fprintf(stderr, "%s\n", status.message.c_str());
            continue;
        }

        if (saveImage(session, fileCounter)) fileCounter++;
    }

    return 0;
}

static std::vector<char *> makeForwardedArgs(int argc, char **argv) {
    std::vector<char *> forwardedArgs;
    forwardedArgs.reserve(static_cast<size_t>(argc));
    forwardedArgs.push_back(argv[0]);

    for (int i = 2; i < argc; i++) {
        forwardedArgs.push_back(argv[i]);
    }

    return forwardedArgs;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        ArgsUsage::printTopLevelUsage(argv[0]);
        return 1;
    }

    const char *backendArg = argv[1];
    if (ArgsUsage::isHelpArg(backendArg)) {
        ArgsUsage::printTopLevelUsage(argv[0]);
        return 0;
    }

    const std::string configName = ArgsUsage::resolveBackend(backendArg);
    if (configName.empty()) {
        fprintf(stderr, "Unknown backend: %s\n\n", backendArg);
        ArgsUsage::printTopLevelUsage(argv[0]);
        return 1;
    }

    std::string error;
    BackendModule backend = loadBackendModule(PathUtil::executableDir(),
        configName, error);
    if (!backend) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    Session *session = backend.makeSession();
    if (!session) {
        fprintf(stderr, "Failed to create backend session.\n");
        return 1;
    }

    CallbackFormatter formatter;
    formatter.bind(*session);

    std::vector<char *> forwardedArgs = makeForwardedArgs(argc, argv);
    if (ArgsParser::checkHelp(static_cast<int>(forwardedArgs.size()),
        forwardedArgs.data())) {
        return 0;
    }

    const CPUInfo cpuInfo = queryCPUInfo();
    printf("CPU: %s (%dc/%dt)\n", cpuInfo.name.c_str(),
        cpuInfo.cores, cpuInfo.threads);

    if (ArgsUsage::argsCount(static_cast<int>(forwardedArgs.size())) == 1 &&
        std::string_view(forwardedArgs[1]) == ArgsUsage::replOption) {
        return runRepl(*session, argv[0]);
    }

    return static_cast<int>(!runOnce(*session,
        static_cast<int>(forwardedArgs.size()),
        forwardedArgs.data()));
}