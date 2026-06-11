#include "wit_infra/ScanProfiler.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace wit::infra {
namespace {

std::wstring ExecutableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::filesystem::current_path().wstring();
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path().wstring();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string EscapeJsonString(const std::wstring& value) {
    std::ostringstream out;
    out << '"';
    for (const wchar_t ch : value) {
        switch (ch) {
        case L'"': out << "\\\""; break;
        case L'\\': out << "\\\\"; break;
        case L'\b': out << "\\b"; break;
        case L'\f': out << "\\f"; break;
        case L'\n': out << "\\n"; break;
        case L'\r': out << "\\r"; break;
        case L'\t': out << "\\t"; break;
        default:
            if (ch >= 0 && ch < 0x20) {
                out << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec << std::nouppercase;
            } else if (ch <= 0x7F) {
                out << static_cast<char>(ch);
            } else if (ch <= 0x7FF) {
                out << static_cast<char>(0xC0 | ((ch >> 6) & 0x1F));
                out << static_cast<char>(0x80 | (ch & 0x3F));
            } else if (ch >= 0xD800 && ch <= 0xDBFF) {
                out << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec << std::nouppercase;
            } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
                out << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec << std::nouppercase;
            } else {
                out << static_cast<char>(0xE0 | ((ch >> 12) & 0x0F));
                out << static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                out << static_cast<char>(0x80 | (ch & 0x3F));
            }
            break;
        }
    }
    out << '"';
    return out.str();
}

std::string EscapeJsonString(const std::string& value) {
    return EscapeJsonString(std::wstring(value.begin(), value.end()));
}

std::filesystem::path ProfilePath(const ScanProfile& profile) {
    const auto directory = std::filesystem::path(ExecutableDirectory()) / L"scan-profiles";
    std::filesystem::create_directories(directory);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wostringstream name;
    name << L"scan-profile-"
        << std::setw(4) << std::setfill(L'0') << time.wYear
        << std::setw(2) << time.wMonth
        << std::setw(2) << time.wDay
        << L"-"
        << std::setw(2) << time.wHour
        << std::setw(2) << time.wMinute
        << std::setw(2) << time.wSecond
        << L"-" << GetCurrentProcessId()
        << L"-" << profile.scanId
        << L".json";
    return directory / name.str();
}

const char* BoolText(bool value) {
    return value ? "true" : "false";
}

}

ScopedScanTimer::ScopedScanTimer(std::uint64_t& accumulator) noexcept
    : accumulator_(&accumulator), start_(std::chrono::steady_clock::now()) {
}

ScopedScanTimer::ScopedScanTimer(ScopedScanTimer&& other) noexcept
    : accumulator_(std::exchange(other.accumulator_, nullptr)), start_(other.start_) {
}

ScopedScanTimer::~ScopedScanTimer() noexcept {
    if (!accumulator_) return;
    const auto elapsed = std::chrono::steady_clock::now() - start_;
    *accumulator_ += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

bool WriteScanProfileJson(const ScanProfile& profile) {
    try {
        std::ofstream out(ProfilePath(profile), std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << "{\n"
            << "  \"schemaVersion\": 1,\n"
            << "  \"scanId\": " << profile.scanId << ",\n"
            << "  \"root\": " << EscapeJsonString(profile.root) << ",\n"
            << "  \"result\": " << EscapeJsonString(profile.result) << ",\n"
            << "  \"options\": {\n"
            << "    \"calculateCrc\": " << BoolText(profile.options.calculateCrc) << ",\n"
            << "    \"browseArchives\": " << BoolText(profile.options.browseArchives) << ",\n"
            << "    \"countFilesBeforeScan\": " << BoolText(profile.options.countFilesBeforeScan) << ",\n"
            << "    \"profilingEnabled\": " << BoolText(profile.options.profilingEnabled) << "\n"
            << "  },\n"
            << "  \"counts\": {\n"
            << "    \"files\": " << profile.counts.files << ",\n"
            << "    \"folders\": " << profile.counts.folders << ",\n"
            << "    \"archiveProbeAttempts\": " << profile.counts.archiveProbeAttempts << ",\n"
            << "    \"archivesRecognized\": " << profile.counts.archivesRecognized << ",\n"
            << "    \"archiveFiles\": " << profile.counts.archiveFiles << ",\n"
            << "    \"archiveFolders\": " << profile.counts.archiveFolders << ",\n"
            << "    \"crcFiles\": " << profile.counts.crcFiles << ",\n"
            << "    \"dbInsertFileCalls\": " << profile.counts.dbInsertFileCalls << ",\n"
            << "    \"dbInsertFolderCalls\": " << profile.counts.dbInsertFolderCalls << ",\n"
            << "    \"dbUpdateFolderContentSizeCalls\": " << profile.counts.dbUpdateFolderContentSizeCalls << ",\n"
            << "    \"progressReports\": " << profile.counts.progressReports << "\n"
            << "  },\n"
            << "  \"bytes\": {\n"
            << "    \"fileBytesSeen\": " << profile.bytes.fileBytesSeen << ",\n"
            << "    \"crcBytesRead\": " << profile.bytes.crcBytesRead << ",\n"
            << "    \"archiveBytesRead\": " << profile.bytes.archiveBytesRead << "\n"
            << "  },\n"
            << "  \"timingsNs\": {\n"
            << "    \"total\": " << profile.timingsNs.total << ",\n"
            << "    \"workingCopyCreate\": " << profile.timingsNs.workingCopyCreate << ",\n"
            << "    \"countFiles\": " << profile.timingsNs.countFiles << ",\n"
            << "    \"beginTransaction\": " << profile.timingsNs.beginTransaction << ",\n"
            << "    \"populateVolumeMetadata\": " << profile.timingsNs.populateVolumeMetadata << ",\n"
            << "    \"findExistingDisk\": " << profile.timingsNs.findExistingDisk << ",\n"
            << "    \"deleteOldDiskContent\": " << profile.timingsNs.deleteOldDiskContent << ",\n"
            << "    \"addDisk\": " << profile.timingsNs.addDisk << ",\n"
            << "    \"scanFolder\": " << profile.timingsNs.scanFolder << ",\n"
            << "    \"directoryEnumeration\": " << profile.timingsNs.directoryEnumeration << ",\n"
            << "    \"metadataBuild\": " << profile.timingsNs.metadataBuild << ",\n"
            << "    \"dbInsertFile\": " << profile.timingsNs.dbInsertFile << ",\n"
            << "    \"dbInsertFolder\": " << profile.timingsNs.dbInsertFolder << ",\n"
            << "    \"dbUpdateFolderContentSize\": " << profile.timingsNs.dbUpdateFolderContentSize << ",\n"
            << "    \"archiveProbe\": " << profile.timingsNs.archiveProbe << ",\n"
            << "    \"archiveRead\": " << profile.timingsNs.archiveRead << ",\n"
            << "    \"crcReadAndCompute\": " << profile.timingsNs.crcReadAndCompute << ",\n"
            << "    \"progressCallbacks\": " << profile.timingsNs.progressCallbacks << ",\n"
            << "    \"updateDisk\": " << profile.timingsNs.updateDisk << ",\n"
            << "    \"updateDiskScanStatistics\": " << profile.timingsNs.updateDiskScanStatistics << ",\n"
            << "    \"commit\": " << profile.timingsNs.commit << ",\n"
            << "    \"rollback\": " << profile.timingsNs.rollback << ",\n"
            << "    \"saveCatalog\": " << profile.timingsNs.saveCatalog << "\n"
            << "  },\n"
            << "  \"sqlite\": {\n"
            << "    \"prepareInsertFile\": " << profile.sqlite.prepareInsertFile << ",\n"
            << "    \"prepareInsertFolder\": " << profile.sqlite.prepareInsertFolder << ",\n"
            << "    \"prepareUpdateFolderContentSize\": " << profile.sqlite.prepareUpdateFolderContentSize << ",\n"
            << "    \"stepInsertFile\": " << profile.sqlite.stepInsertFile << ",\n"
            << "    \"stepInsertFolder\": " << profile.sqlite.stepInsertFolder << ",\n"
            << "    \"stepUpdateFolderContentSize\": " << profile.sqlite.stepUpdateFolderContentSize << ",\n"
            << "    \"resetInsertFile\": " << profile.sqlite.resetInsertFile << ",\n"
            << "    \"resetInsertFolder\": " << profile.sqlite.resetInsertFolder << ",\n"
            << "    \"resetUpdateFolderContentSize\": " << profile.sqlite.resetUpdateFolderContentSize << ",\n"
            << "    \"finalizeInsertFile\": " << profile.sqlite.finalizeInsertFile << ",\n"
            << "    \"finalizeInsertFolder\": " << profile.sqlite.finalizeInsertFolder << ",\n"
            << "    \"finalizeUpdateFolderContentSize\": " << profile.sqlite.finalizeUpdateFolderContentSize << "\n"
            << "  }\n"
            << "}\n";
        return out.good();
    } catch (...) {
        return false;
    }
}

}
