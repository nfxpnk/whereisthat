# Changelog

All notable changes to WhereIsThat will be documented in this file.

The project uses semantic versioning where possible:
- PATCH versions are for bug fixes.
- MINOR versions are for new features.
- MAJOR versions are for major stable or incompatible changes.

## [0.1.2] - 2026-06-14

### Added
- Added natural sorting for catalog roots, folder contents, and search results, including toolbar and column-header sorting controls.
- Added context menus for tree, file-list, and search-result items with catalog, group, disk, folder, file, and Explorer actions.
- Added search-result "Locate in Catalog" support and file-list "Go To..." navigation for folders.
- Added multi-catalog session persistence, including reopening previously open catalogs and restoring the last active catalog.
- Added file-list column-width persistence and saved content sort preferences.

### Changed
- Moved application logs into a dedicated `logs` folder.
- Centered the main window on startup.
- Updated archive scanning build and packaging documentation for the vendored libarchive runtime.

### Fixed
- Prevented right-click selection from opening item contents.
- Fixed sorting behavior and selection/focus restoration after resorting list contents.
- Improved storage smoke coverage for persisted catalog session settings.

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
