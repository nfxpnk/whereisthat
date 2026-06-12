# Changelog

All notable changes to WhereIsThat will be documented in this file.

The project uses semantic versioning where possible:
- PATCH versions are for bug fixes.
- MINOR versions are for new features.
- MAJOR versions are for major stable or incompatible changes.

## [0.1.1] - 2026-06-12

### Added
- Added general settings, scan options, and alpha-warning preference support.
- Added startup catalog opening from a command-line path.
- Added CRC32 coverage, scan profiling, and catalog test tooling.

### Changed
- Improved scan performance, list/search scrolling, and SQLite error handling.

## [0.1.0] - 2026-06-05

### Added
- Initial Windows x64 release packaging workflow.
- Version tracking through the root `VERSION` file.
- Automatic GitHub Release creation from version tags.

### Known limitations
- Search currently supports literal item-name substring matching only.
- Some menu and toolbar commands are placeholders.
