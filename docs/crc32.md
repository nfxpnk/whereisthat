# CRC-32 File Scanning

## Selected approach

WhereIsThat stores standard reflected IEEE CRC-32 values, matching zlib CRC-32 output. The current implementation uses an in-tree table-driven CRC-32 calculator and a Windows `ReadFile` file path with `FILE_FLAG_SEQUENTIAL_SCAN`.

zlib was the preferred library candidate because `crc32()` / `crc32_z()` match the required CRC-32 variant and zlib is mature and permissively licensed. The repository currently ships zlib only as `third_party/libarchive/bin/z.dll`, a runtime dependency of libarchive, without a zlib header or import library for direct application use. Adding a partial ad hoc dynamic load of that transitive DLL would make scanner correctness depend on libarchive packaging details, so this change keeps CRC calculation self-contained.

Intel ISA-L was not selected for this pass. It is capable of fast CRC functions, including gzip/IEEE-style CRC, but it would add a larger native dependency and platform-specific packaging work that is not justified until profiling shows CRC computation, rather than file I/O and file-open overhead, is still the dominant scan cost.

CRC32C hardware instructions are not compatible with the stored CRC-32 format because CRC32C uses a different polynomial.

## Implementation notes

- `wit::core::Crc32` updates buffers using slicing-by-8 lookup tables generated at compile time from the reflected IEEE polynomial `0xEDB88320`.
- `CalculateFileCrc32()` opens Unicode Windows paths with `CreateFileW`, uses `FILE_FLAG_SEQUENTIAL_SCAN`, and reads through a 1 MiB thread-local reusable buffer to avoid per-file allocation churn during large scans.
- Empty files return `00000000`.
- Missing or unreadable files return `std::nullopt`; cooperative cancellation can be distinguished via the optional `cancelled` out parameter.
- The same CRC accumulator is used for archive members and ordinary filesystem files.

## Benchmark command

```powershell
msbuild UnitTests.vcxproj /p:Configuration=Release /p:Platform=x64 /m
x64\Release\UnitTests.exe --gtest_also_run_disabled_tests --gtest_filter=Crc32Benchmark.*
```

## Local benchmark results

Measured on June 5, 2026 with the Release x64 test binary. The first warm run includes more filesystem cache/setup variance; the repeat run is the best comparison for CRC throughput.

| Workload | New repeat | Old bitwise | Notes |
| --- | ---: | ---: | --- |
| 1000 tiny files, 32 B each | 5627 files/sec | 6039 files/sec | File open overhead dominates; CRC algorithm is effectively irrelevant. |
| 512 small files, 16 KiB each | 89.9 MiB/sec | 30.8 MiB/sec | New path is about 2.9x faster. |
| 16 medium files, 8 MiB each | 666 MiB/sec | 56.0 MiB/sec | New path is about 11.9x faster. |
| 1 large file, 256 MiB | 773 MiB/sec | Not run | Disk/cache throughput dominates enough that the old loop was skipped to keep benchmark runtime short. |

These numbers should be treated as machine-local, warm-cache guidance. For real-world disk scans, tiny-file workloads are usually limited by file enumeration and open/read latency, while larger files become limited by disk/cache throughput once CRC calculation is no longer byte/bit-loop bound.
