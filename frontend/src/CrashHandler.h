#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "util/IncludeWin32.h"

#include <DbgHelp.h>
#include <Psapi.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

#include "util/DateTimeUtil.h"

namespace _CrashHandlerImpl {
    inline void writeCrashReport(const std::string &report) {
        std::error_code err;
        std::filesystem::create_directories("logs", err);
        if (err) return;

        const std::filesystem::path logPath = std::filesystem::path("logs")
            / ("crash_" + DateTimeUtil::formatCurrentLocalTime("%Y_%m_%d-%H_%M_%S") + ".log");

        std::ofstream file(logPath, std::ios::binary);
        if (file) file.write(report.data(), static_cast<std::streamsize>(report.size()));
    }

    inline void displayCrashReport(const std::string &report) {
        std::string display = report;
        const size_t MB_LIMIT = 30000;
        if (display.size() > MB_LIMIT) {
            display.resize(MB_LIMIT);
            display += "\n...[truncated, see log file]";
        }

#ifdef CRASH_HANDLER_MSGBOX
        MessageBoxA(NULL, display.c_str(), "Crash Detected", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
#elif defined(CRASH_HANDLER_PRINT)
        fprintf(stderr, "%s\n", display.c_str());
        fflush(stderr);
#else
#error "No reporting type selected"
#endif
    }

    inline bool AddressInModule(uintptr_t addr, const char *moduleName) {
        HMODULE hMod = GetModuleHandleA(moduleName);
        if (!hMod) return false;
        MODULEINFO mi = {};
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return false;
        uintptr_t start = (uintptr_t)mi.lpBaseOfDll;
        uintptr_t end = start + (uintptr_t)mi.SizeOfImage;
        return addr >= start && addr < end;
    }

    inline LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS *exceptionInfo) {
        EXCEPTION_RECORD *excRecord = exceptionInfo->ExceptionRecord;
        CONTEXT *context = exceptionInfo->ContextRecord;

        if (!excRecord->ExceptionAddress)
            return EXCEPTION_CONTINUE_SEARCH;

        switch (excRecord->ExceptionCode) {
            case 0x40010006:
            case 0x4001000A:
            case 0x406D1388:
            case 0xE06D7363:
            case 0x80000003:
            case 0xC000001D:
            case 0x80000004:
                return EXCEPTION_CONTINUE_SEARCH;
        }

        uintptr_t excAddr = (uintptr_t)excRecord->ExceptionAddress;

        if (AddressInModule(excAddr, "ntdll.dll") ||
            AddressInModule(excAddr, "RobloxPlayerBeta.dll"))
            return EXCEPTION_CONTINUE_SEARCH;

        HANDLE process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);

        const char *exceptionType;
        switch (excRecord->ExceptionCode) {
            case EXCEPTION_ACCESS_VIOLATION:      exceptionType = "Access Violation";       break;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: exceptionType = "Array Bounds Exceeded";  break;
            case EXCEPTION_BREAKPOINT:            exceptionType = "Breakpoint";             break;
            case EXCEPTION_DATATYPE_MISALIGNMENT: exceptionType = "Datatype Misalignment";  break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:    exceptionType = "Float Divide by Zero";   break;
            case EXCEPTION_FLT_OVERFLOW:          exceptionType = "Float Overflow";         break;
            case EXCEPTION_FLT_UNDERFLOW:         exceptionType = "Float Underflow";        break;
            case EXCEPTION_ILLEGAL_INSTRUCTION:   exceptionType = "Illegal Instruction";    break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:    exceptionType = "Integer Divide by Zero"; break;
            case EXCEPTION_PRIV_INSTRUCTION:      exceptionType = "Privileged Instruction"; break;
            case EXCEPTION_STACK_OVERFLOW:        exceptionType = "Stack Overflow";         break;
            default:                              exceptionType = "Unknown Exception";      break;
        }

        std::ostringstream oss;
        oss << std::hex << std::uppercase;

        oss << "=== CRASH REPORT ===\n\n";
        oss << "Code    : 0x" << excRecord->ExceptionCode << "  (" << exceptionType << ")\n";
        oss << "Address : 0x" << excAddr << "\n";

        if (excRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            excRecord->NumberParameters >= 2) {
            oss << "AV Type : " << (excRecord->ExceptionInformation[0] ? "Write" : "Read") << "\n";
            oss << "AV Addr : 0x" << excRecord->ExceptionInformation[1] << "\n";
        }

        oss << "\n--- Registers ---\n";
        oss << "RAX=" << context->Rax << "  RBX=" << context->Rbx << "\n";
        oss << "RCX=" << context->Rcx << "  RDX=" << context->Rdx << "\n";
        oss << "RSI=" << context->Rsi << "  RDI=" << context->Rdi << "\n";
        oss << "RSP=" << context->Rsp << "  RBP=" << context->Rbp << "\n";
        oss << "RIP=" << context->Rip << "\n";

        STACKFRAME64 sf = {};
        sf.AddrPC.Offset = context->Rip;  sf.AddrPC.Mode = AddrModeFlat;
        sf.AddrFrame.Offset = context->Rbp;  sf.AddrFrame.Mode = AddrModeFlat;
        sf.AddrStack.Offset = context->Rsp;  sf.AddrStack.Mode = AddrModeFlat;

        oss << "\n--- Call Stack ---\n";

        int frameIdx = 0;
        while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(),
            &sf, context, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            if (sf.AddrPC.Offset == 0) break;
            uintptr_t frameAddr = (uintptr_t)sf.AddrPC.Offset;

            char modName[MAX_PATH] = "<unknown>";
            DWORD64 modBase = SymGetModuleBase64(process, (DWORD64)frameAddr);
            if (modBase)
                GetModuleFileNameA((HMODULE)modBase, modName, MAX_PATH);

            const char *modShort = strrchr(modName, '\\');
            modShort = modShort ? modShort + 1 : modName;

            uintptr_t rva = modBase ? (frameAddr - (uintptr_t)modBase) : frameAddr;

            char symBuf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
            PSYMBOL_INFO sym = (PSYMBOL_INFO)symBuf;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = MAX_SYM_NAME;
            DWORD64 symDisp = 0;

            char funcName[512] = "<unknown>";
            if (SymFromAddr(process, (DWORD64)frameAddr, &symDisp, sym))
                sprintf(funcName, "%s+0x%llX", sym->Name, symDisp);

            char srcInfo[512] = "";
            IMAGEHLP_LINE64 line = {};
            DWORD lineDisp = 0;
            line.SizeOfStruct = sizeof(line);
            if (SymGetLineFromAddr64(process, (DWORD64)frameAddr, &lineDisp, &line))
                sprintf(srcInfo, " [%s:%lu]", line.FileName, line.LineNumber);

            oss << std::dec << "#" << frameIdx++ << " ";
            oss << std::hex
                << "VA=0x" << frameAddr
                << " RVA=0x" << rva
                << "  " << modShort
                << "!" << funcName
                << srcInfo << "\n";
        }

        std::string report = oss.str();
        writeCrashReport(report);
        displayCrashReport(report);

        return EXCEPTION_CONTINUE_SEARCH;
    }

    inline PVOID continueHandler = nullptr;
    inline PVOID exceptionHandler = nullptr;
}

inline void installCrashHandler() {
    if (!_CrashHandlerImpl::continueHandler) {
        _CrashHandlerImpl::continueHandler = AddVectoredContinueHandler(1, _CrashHandlerImpl::ExceptionFilter);
    }

    if (!_CrashHandlerImpl::exceptionHandler) {
        _CrashHandlerImpl::exceptionHandler = AddVectoredExceptionHandler(1, _CrashHandlerImpl::ExceptionFilter);
    }

    SetUnhandledExceptionFilter(_CrashHandlerImpl::ExceptionFilter);
}

inline void uninstallCrashHandler() {
    if (_CrashHandlerImpl::continueHandler) {
        RemoveVectoredContinueHandler(_CrashHandlerImpl::continueHandler);
        _CrashHandlerImpl::continueHandler = nullptr;
    }

    if (_CrashHandlerImpl::exceptionHandler) {
        RemoveVectoredExceptionHandler(_CrashHandlerImpl::exceptionHandler);
        _CrashHandlerImpl::exceptionHandler = nullptr;
    }

    SetUnhandledExceptionFilter(nullptr);
}
