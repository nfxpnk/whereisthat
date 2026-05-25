## 1. Dedicated General Settings UI Component

- [x] 1.1 Add `src/ui/GeneralSettingsDialog.h` and `src/ui/GeneralSettingsDialog.cpp` as an `ATL::CDialogImpl`-based native modal dialog using `IDD_GENERAL_SETTINGS` and WTL/ATL message-map handling.
- [x] 1.2 Move initialization of `Show status bar`, display of the read-only last-opened catalog path, and `OK`/`Cancel` value handling into the dedicated dialog component while preserving current behavior.
- [x] 1.3 Add the new dialog header and source file to `WhereIsThat.vcxproj` without introducing any new library or runtime dependency.

## 2. Main-Frame Coordination Integration

- [x] 2.1 Remove `GeneralSettingsDialogData`, `GeneralSettingsDialogProc`, and raw `DialogBoxParamW` dispatch for General Settings from `src/app/MainFrame.cpp`.
- [x] 2.2 Update `MainFrame::OnGeneralSettings()` to invoke `wit::ui::GeneralSettingsDialog`, persist only confirmed results through `CatalogSession::SaveSettings()`, retain save-error reporting, and apply the successfully saved status-bar visibility.
- [x] 2.3 Confirm existing resource IDs, Options command routing, `settings.ini` keys/defaults, and last-opened catalog display require no behavioral or persisted-data migration.

## 3. Documentation And Verification

- [x] 3.1 Update maintained UI/architecture documentation where needed to identify General Settings as a dedicated WTL/ATL-hosted dialog within `src/ui` while preserving its `settings.ini` contract.
- [x] 3.2 Build `whereisthat.sln` for `Release|x64` through MSBuild and confirm the new WTL/ATL dialog files compile with the existing native dependency set.
- [ ] 3.3 Smoke-test opening General Settings from Options, initial and persisted `Show status bar` state, read-only last-opened catalog display, confirm/apply behavior, Cancel behavior, and absence of catalog data changes.
