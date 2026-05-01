#define CRASH_HANDLER_MSGBOX
#include "CrashHandler.h"

#pragma section(".CRT$XIB", long, read)

extern "C" int EarlyInit() {
    installCrashHandler();
    return 0;
}

extern "C" __declspec(allocate(".CRT$XIB"))
int (*EarlyInitPtr)() = EarlyInit;

#ifdef _WIN64
#pragma comment(linker, "/include:EarlyInitPtr")
#else
#pragma comment(linker, "/include:_EarlyInitPtr")
#endif