#pragma once

#include <string_view>

namespace wit::infra {

enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error
};

void InitializeLogging();
void ShutdownLogging();

void Log(
    LogLevel level,
    const wchar_t* file,
    int line,
    const wchar_t* function,
    std::wstring_view message);

} // namespace wit::infra

#define WIT_LOG_TRACE(msg) \
    ::wit::infra::Log(::wit::infra::LogLevel::trace, __FILEW__, __LINE__, __FUNCTIONW__, msg)

#define WIT_LOG_DEBUG(msg) \
    ::wit::infra::Log(::wit::infra::LogLevel::debug, __FILEW__, __LINE__, __FUNCTIONW__, msg)

#define WIT_LOG_INFO(msg) \
    ::wit::infra::Log(::wit::infra::LogLevel::info, __FILEW__, __LINE__, __FUNCTIONW__, msg)

#define WIT_LOG_WARN(msg) \
    ::wit::infra::Log(::wit::infra::LogLevel::warning, __FILEW__, __LINE__, __FUNCTIONW__, msg)

#define WIT_LOG_ERROR(msg) \
    ::wit::infra::Log(::wit::infra::LogLevel::error, __FILEW__, __LINE__, __FUNCTIONW__, msg)
