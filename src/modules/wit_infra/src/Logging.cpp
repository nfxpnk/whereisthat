#include "wit_infra/Logging.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <mutex>
#include <string>

namespace wit::infra {
namespace {

std::mutex g_logMutex;
HANDLE g_logFile = INVALID_HANDLE_VALUE;
std::wstring g_logPath;
std::wstring g_logDirectory;
LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter{};
bool g_initialized{};

const wchar_t* LevelText(LogLevel level) {
    switch (level) {
    case LogLevel::trace:
        return L"TRACE";
    case LogLevel::debug:
        return L"DEBUG";
    case LogLevel::info:
        return L"INFO";
    case LogLevel::warning:
        return L"WARN";
    case LogLevel::error:
        return L"ERROR";
    default:
        return L"UNKNOWN";
    }
}

std::wstring LogDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
    std::filesystem::path base = (length != 0 && length < std::size(modulePath))
        ? std::filesystem::path(modulePath).parent_path()
        : std::filesystem::current_path();
    return base.wstring();
}

std::wstring TimestampForFile() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    return std::format(L"{:04}{:02}{:02}-{:02}{:02}{:02}-{}",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
}

std::wstring TimestampForLine() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    return std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
}

std::wstring LeafName(std::wstring_view path) {
    const auto slash = path.find_last_of(L"\\/");
    return std::wstring(slash == std::wstring_view::npos ? path : path.substr(slash + 1));
}

void WriteLineUnlocked(std::wstring_view line) {
    if (g_logFile == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    WriteFile(g_logFile, line.data(), static_cast<DWORD>(line.size() * sizeof(wchar_t)), &written, nullptr);
    const wchar_t newline[] = L"\r\n";
    WriteFile(g_logFile, newline, static_cast<DWORD>((std::size(newline) - 1) * sizeof(wchar_t)), &written, nullptr);
    FlushFileBuffers(g_logFile);
}

std::wstring LastErrorText(DWORD error) {
    wchar_t* buffer{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length != 0 && buffer ? std::wstring(buffer, length) : std::format(L"error {}", error);
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) message.pop_back();
    return message;
}

LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD exceptionCode = exceptionInfo && exceptionInfo->ExceptionRecord
        ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
    std::wstring dumpPath;
    {
        std::lock_guard lock(g_logMutex);
        dumpPath = (std::filesystem::path(g_logDirectory) /
            (L"whereisthat-crash-" + TimestampForFile() + L".dmp")).wstring();
        WriteLineUnlocked(std::format(L"{} [ERROR] unhandled exception 0x{:08X}; writing dump to {}",
            TimestampForLine(), exceptionCode, dumpPath));
    }

    HANDLE dumpFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpException{};
        dumpException.ThreadId = GetCurrentThreadId();
        dumpException.ExceptionPointers = exceptionInfo;
        dumpException.ClientPointers = FALSE;
        const BOOL dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
            MiniDumpWithDataSegs, exceptionInfo ? &dumpException : nullptr, nullptr, nullptr);
        CloseHandle(dumpFile);
        if (!dumped) {
            const auto error = GetLastError();
            std::lock_guard lock(g_logMutex);
            WriteLineUnlocked(std::format(L"{} [ERROR] MiniDumpWriteDump failed: {}",
                TimestampForLine(), LastErrorText(error)));
        }
    } else {
        const auto error = GetLastError();
        std::lock_guard lock(g_logMutex);
        WriteLineUnlocked(std::format(L"{} [ERROR] could not create dump file: {}",
            TimestampForLine(), LastErrorText(error)));
    }

    if (g_previousExceptionFilter) return g_previousExceptionFilter(exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

void InitializeLoggingUnlocked() {
    if (g_initialized) return;

    g_logDirectory = LogDirectory();
    std::error_code ignored;
    std::filesystem::create_directories(g_logDirectory, ignored);
    g_logPath = (std::filesystem::path(g_logDirectory) /
        (L"whereisthat-" + TimestampForFile() + L".log")).wstring();

    g_logFile = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_logFile != INVALID_HANDLE_VALUE) {
        constexpr wchar_t bom = 0xFEFF;
        DWORD written{};
        WriteFile(g_logFile, &bom, sizeof(bom), &written, nullptr);
        WriteLineUnlocked(std::format(L"{} [INFO] logging initialized: {}", TimestampForLine(), g_logPath));
    }
    g_previousExceptionFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);
    g_initialized = true;
}

}

void InitializeLogging() {
    std::lock_guard lock(g_logMutex);
    InitializeLoggingUnlocked();
}

void ShutdownLogging() {
    std::lock_guard lock(g_logMutex);
    if (!g_initialized) return;
    WriteLineUnlocked(std::format(L"{} [INFO] logging shutdown", TimestampForLine()));
    if (g_logFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }
    g_initialized = false;
}

void Log(LogLevel level, const wchar_t* file, int line, const wchar_t* function, std::wstring_view message) {
    std::lock_guard lock(g_logMutex);
    InitializeLoggingUnlocked();
    const auto formatted = std::format(L"{} [{}] {}:{} {}: {}",
        TimestampForLine(), LevelText(level), LeafName(file ? file : L""), line,
        function ? function : L"", message);
    WriteLineUnlocked(formatted);
    OutputDebugStringW((formatted + L"\n").c_str());
}

} // namespace wit::infra
