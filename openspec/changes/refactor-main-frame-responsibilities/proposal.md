## Why

`MainFrame.cpp` is now more than 1,100 lines and combines native window hosting, layout and presentation, navigation, catalog-session state, asynchronous scanning, status rendering, and command workflows. Establishing explicit component boundaries now is necessary before migrating the remaining main-window code to WTL, otherwise the migration would preserve the same oversized ownership model behind new wrappers.

## What Changes

- Split the current main-frame implementation into four or five focused components, leaving `MainFrame` as the top-level native window host and message/command coordinator.
- Extract browser navigation and view coordination, catalog session and pending-save lifecycle, asynchronous scan coordination, and main-window chrome/status/layout behavior from the frame into separately owned classes.
- Preserve existing native UI behavior, catalog persistence semantics, scan-thread message marshalling, toolbar/menu routes, status-bar output, and browser interaction during the refactor.
- Add a persistent architectural rule requiring future main-window work to extend the responsible component or create an appropriately scoped component instead of accumulating unrelated responsibilities in `MainFrame`.
- Record that WTL 10.01 is already vendored and used for application/dialog infrastructure; converting the remaining frame/control implementation to WTL is a subsequent change built on these boundaries, not part of this behavioral-preserving extraction.

## Capabilities

### New Capabilities

- `main-frame-decomposition`: Defines maintainable main-window responsibility boundaries, the thin-frame rule, and preservation of observable behavior while responsibilities are extracted.

### Modified Capabilities

None. Existing UI and catalog workflow requirements are preserved.

## Impact

- `src/app/MainFrame.cpp` and `src/app/MainFrame.h` will shrink to top-level lifetime, essential dispatch, and composition responsibilities.
- New focused classes under `src/app` and/or `src/ui` will own layout/chrome, browser coordination, catalog session lifecycle, and scanning workflow behavior according to existing layer boundaries.
- `WhereIsThat.vcxproj` will include any new compilation units; no new runtime or database dependency is introduced.
- `docs/spec/architecture.md` and `docs/spec/coding-rules.md` will gain the enforceable main-frame responsibility rule and the sequencing note for the later WTL frame migration.
