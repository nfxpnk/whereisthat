#include <gtest/gtest.h>
#include <wit_scanner/Crc32.h>

#include <Windows.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
class TempTree {
public:
    TempTree() : root_(std::filesystem::temp_directory_path() /
        (L"whereisthat-crc32-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()))) {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~TempTree() {
        std::filesystem::remove_all(root_);
    }

    [[nodiscard]] std::filesystem::path Path(const wchar_t* name) const {
        return root_ / name;
    }

private:
    std::filesystem::path root_;
};

void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good());
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(file.good());
}

void WriteString(const std::filesystem::path& path, std::string_view contents) {
    WriteBytes(path, std::as_bytes(std::span{contents.data(), contents.size()}));
}

std::uint32_t OldBitwiseCrc32(std::span<const std::byte> bytes) {
    auto crc = 0xFFFFFFFFu;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

std::vector<std::byte> MakeData(std::size_t size) {
    std::vector<std::byte> data(size);
    std::mt19937 generator(0x574954u);
    std::uniform_int_distribution<int> bytes(0, 255);
    for (auto& value : data) value = static_cast<std::byte>(bytes(generator));
    return data;
}

std::uint32_t ReadOldBitwiseFileCrc32(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return OldBitwiseCrc32(bytes);
}

struct BenchResult {
    std::size_t files{};
    std::uint64_t bytes{};
    std::chrono::duration<double> elapsed{};
};

BenchResult BenchmarkFiles(const std::vector<std::filesystem::path>& paths) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t bytes{};
    for (const auto& path : paths) {
        const auto crc = wit::core::CalculateFileCrc32(path);
        EXPECT_TRUE(crc.has_value()) << path.wstring();
        bytes += std::filesystem::file_size(path);
    }
    return {paths.size(), bytes, std::chrono::steady_clock::now() - start};
}

void PrintBenchResult(const char* label, const BenchResult& result) {
    const auto seconds = result.elapsed.count();
    const auto mib = static_cast<double>(result.bytes) / (1024.0 * 1024.0);
    std::cout << label << ": " << result.files << " files, " << mib << " MiB, "
        << (seconds == 0.0 ? 0.0 : result.files / seconds) << " files/sec, "
        << (seconds == 0.0 ? 0.0 : mib / seconds) << " MiB/sec, "
        << (result.files == 0 ? 0.0 : seconds * 1000.0 / result.files) << " ms/file\n";
}
}

TEST(Crc32, CalculatesKnownVectors) {
    const std::string empty;
    const std::string canonical = "123456789";
    const std::string abc = "abc";

    wit::core::Crc32 emptyCrc;
    emptyCrc.Update(std::as_bytes(std::span{empty.data(), empty.size()}));
    EXPECT_EQ(emptyCrc.Finalize(), 0x00000000u);

    wit::core::Crc32 canonicalCrc;
    canonicalCrc.Update(std::as_bytes(std::span{canonical.data(), canonical.size()}));
    EXPECT_EQ(canonicalCrc.Finalize(), 0xCBF43926u);

    wit::core::Crc32 abcCrc;
    abcCrc.Update(std::as_bytes(std::span{abc.data(), abc.size()}));
    EXPECT_EQ(abcCrc.Finalize(), 0x352441C2u);
    EXPECT_EQ(wit::core::FormatCrc32(abcCrc.Finalize()), L"352441C2");
}

TEST(Crc32, CalculatesFilesAndMatchesPreviousImplementation) {
    TempTree tree;
    const auto emptyPath = tree.Path(L"empty.bin");
    const auto canonicalPath = tree.Path(L"123456789.txt");
    const auto randomPath = tree.Path(L"random.bin");
    const auto largePath = tree.Path(L"large.bin");
    const auto unicodePath = tree.Path(L"unicode-\x03c0-\x0444.bin");

    WriteString(emptyPath, "");
    WriteString(canonicalPath, "123456789");
    const auto randomData = MakeData(8192);
    const auto largeData = MakeData(4 * 1024 * 1024 + 17);
    WriteBytes(randomPath, randomData);
    WriteBytes(largePath, largeData);
    WriteString(unicodePath, "unicode path payload");

    EXPECT_EQ(wit::core::CalculateFileCrc32(emptyPath), 0x00000000u);
    EXPECT_EQ(wit::core::CalculateFileCrc32(canonicalPath), 0xCBF43926u);
    EXPECT_EQ(wit::core::CalculateFileCrc32(randomPath), OldBitwiseCrc32(randomData));
    EXPECT_EQ(wit::core::CalculateFileCrc32(largePath), OldBitwiseCrc32(largeData));
    EXPECT_EQ(wit::core::CalculateFileCrc32(unicodePath), ReadOldBitwiseFileCrc32(unicodePath));
}

TEST(Crc32, ReportsMissingFilesWithoutAmbiguousCrc) {
    TempTree tree;
    bool cancelled = true;
    const auto crc = wit::core::CalculateFileCrc32(tree.Path(L"missing.bin"), {}, &cancelled);

    EXPECT_FALSE(crc.has_value());
    EXPECT_FALSE(cancelled);
}

TEST(Crc32Benchmark, DISABLED_FileScanWorkloads) {
    TempTree tree;
    struct Workload {
        const wchar_t* directoryName;
        const char* label;
        std::size_t fileCount;
        std::size_t fileSize;
    };
    const Workload workloads[] = {
        {L"tiny", "tiny", 1000, 32},
        {L"small", "small", 512, 16 * 1024},
        {L"medium", "medium", 16, 8 * 1024 * 1024},
        {L"large", "large", 1, 256 * 1024 * 1024},
    };

    for (const auto& workload : workloads) {
        std::vector<std::filesystem::path> paths;
        paths.reserve(workload.fileCount);
        const auto directory = tree.Path(workload.directoryName);
        std::filesystem::create_directories(directory);
        const auto data = MakeData(workload.fileSize);
        for (std::size_t index = 0; index < workload.fileCount; ++index) {
            auto path = directory / (std::to_wstring(index) + L".bin");
            WriteBytes(path, data);
            paths.push_back(std::move(path));
        }

        const auto warm = BenchmarkFiles(paths);
        PrintBenchResult(std::string("warm ").append(workload.label).c_str(), warm);
        const auto secondWarm = BenchmarkFiles(paths);
        PrintBenchResult(std::string("warm-repeat ").append(workload.label).c_str(), secondWarm);

        if (workload.fileSize <= 8 * 1024 * 1024) {
            const auto start = std::chrono::steady_clock::now();
            for (const auto& path : paths) {
                EXPECT_NE(ReadOldBitwiseFileCrc32(path), 0xFFFFFFFFu);
            }
            const BenchResult old{paths.size(), warm.bytes, std::chrono::steady_clock::now() - start};
            PrintBenchResult(std::string("old-bitwise ").append(workload.label).c_str(), old);
        }
    }
}
