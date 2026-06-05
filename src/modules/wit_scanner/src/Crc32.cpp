#include "wit_scanner/Crc32.h"

#include <Windows.h>
#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <vector>

namespace wit::core {
namespace {
constexpr std::uint32_t kPolynomial = 0xEDB88320u;
constexpr DWORD kFileFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
constexpr std::size_t kBufferSize = 1024 * 1024;

using CrcTable = std::array<std::array<std::uint32_t, 256>, 8>;

constexpr CrcTable MakeCrcTable() {
    CrcTable table{};
    for (std::uint32_t value = 0; value < 256; ++value) {
        auto crc = value;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? kPolynomial : 0u);
        }
        table[0][value] = crc;
    }
    for (std::size_t slice = 1; slice < table.size(); ++slice) {
        for (std::uint32_t value = 0; value < 256; ++value) {
            const auto crc = table[slice - 1][value];
            table[slice][value] = (crc >> 8) ^ table[0][crc & 0xFFu];
        }
    }
    return table;
}

constexpr auto kCrcTable = MakeCrcTable();

void UpdateCrc(std::uint32_t& crc, const std::byte* bytes, std::size_t count) {
    auto* current = reinterpret_cast<const unsigned char*>(bytes);
    while (count >= 8) {
        const auto one = static_cast<std::uint32_t>(current[0]) |
            (static_cast<std::uint32_t>(current[1]) << 8) |
            (static_cast<std::uint32_t>(current[2]) << 16) |
            (static_cast<std::uint32_t>(current[3]) << 24);
        const auto two = static_cast<std::uint32_t>(current[4]) |
            (static_cast<std::uint32_t>(current[5]) << 8) |
            (static_cast<std::uint32_t>(current[6]) << 16) |
            (static_cast<std::uint32_t>(current[7]) << 24);
        crc ^= one;
        crc = kCrcTable[7][crc & 0xFFu] ^
            kCrcTable[6][(crc >> 8) & 0xFFu] ^
            kCrcTable[5][(crc >> 16) & 0xFFu] ^
            kCrcTable[4][crc >> 24] ^
            kCrcTable[3][two & 0xFFu] ^
            kCrcTable[2][(two >> 8) & 0xFFu] ^
            kCrcTable[1][(two >> 16) & 0xFFu] ^
            kCrcTable[0][two >> 24];
        current += 8;
        count -= 8;
    }
    while (count != 0) {
        crc = (crc >> 8) ^ kCrcTable[0][(crc ^ *current) & 0xFFu];
        ++current;
        --count;
    }
}
}

void Crc32::Update(std::span<const std::byte> bytes) {
    UpdateCrc(crc_, bytes.data(), bytes.size());
}

std::uint32_t Crc32::Finalize() const {
    return crc_ ^ 0xFFFFFFFFu;
}

std::wstring FormatCrc32(std::uint32_t crc) {
    std::wostringstream stream;
    stream << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << crc;
    return stream.str();
}

std::optional<std::uint32_t> CalculateFileCrc32(
    const std::filesystem::path& path,
    std::stop_token stopToken,
    bool* cancelled) {
    if (cancelled) *cancelled = false;
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        kFileFlags, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;

    thread_local std::vector<std::byte> buffer(kBufferSize);
    Crc32 crc;
    DWORD count{};
    bool success = true;
    for (;;) {
        if (stopToken.stop_requested()) {
            if (cancelled) *cancelled = true;
            success = false;
            break;
        }
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            success = false;
            break;
        }
        if (count == 0) break;
        crc.Update(std::span<const std::byte>{buffer.data(), static_cast<std::size_t>(count)});
    }
    CloseHandle(file);
    if (!success) return std::nullopt;
    return crc.Finalize();
}

std::optional<std::wstring> CalculateFileCrc32Text(
    const std::filesystem::path& path,
    std::stop_token stopToken,
    bool* cancelled) {
    const auto crc = CalculateFileCrc32(path, stopToken, cancelled);
    if (!crc) return std::nullopt;
    return FormatCrc32(*crc);
}

}
