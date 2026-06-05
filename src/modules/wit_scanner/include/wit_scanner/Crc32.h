#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

namespace wit::core {

class Crc32 {
public:
    void Update(std::span<const std::byte> bytes);
    [[nodiscard]] std::uint32_t Finalize() const;

private:
    std::uint32_t crc_ = 0xFFFFFFFFu;
};

[[nodiscard]] std::wstring FormatCrc32(std::uint32_t crc);

[[nodiscard]] std::optional<std::uint32_t> CalculateFileCrc32(
    const std::filesystem::path& path,
    std::stop_token stopToken = {},
    bool* cancelled = nullptr);

[[nodiscard]] std::optional<std::wstring> CalculateFileCrc32Text(
    const std::filesystem::path& path,
    std::stop_token stopToken = {},
    bool* cancelled = nullptr);

}
