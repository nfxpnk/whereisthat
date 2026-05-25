## Context

`Options > General Settings` is routed in `MainFrame`, and the resource-backed dialog already exposes `Show status bar` plus a read-only last-opened catalog path. Its implementation is the remaining inline dialog exception: `MainFrame.cpp` declares `GeneralSettingsDialogData`, owns a raw `INT_PTR CALLBACK` procedure, calls `DialogBoxParamW`, then saves returned values through `CatalogSession`.

The application now uses vendored WTL 10.01 with MSVC ATL: `MainFrame` is hosted by `WTL::CFrameWindowImpl`, and `AddNewDiskMediaDialog` and `SearchDialog` already use `ATL::CDialogImpl` message maps under `src/ui`. General Settings should follow that existing UI ownership and native dialog pattern without changing its persisted preference contract.

## Goals / Non-Goals

**Goals:**
- Move General Settings modal presentation and control state into dedicated `src/ui/GeneralSettingsDialog.h` and `.cpp` files.
- Host the dialog using the existing WTL/ATL dialog message-map infrastructure.
- Leave `MainFrame` with command coordination only: supply current settings, receive confirmed edited settings, persist via `CatalogSession`, and refresh visible chrome.
- Preserve the existing dialog resource, preference meanings, confirmation/cancellation behavior, and `settings.ini` storage contract.

**Non-Goals:**
- Add General Settings fields, change its layout, or redesign the Options menu.
- Move persistence ownership into the dialog or bypass `CatalogSession`.
- Convert filesystem pickers, message boxes, Common Controls view adapters, or all remaining native API calls to wrappers.
- Change the catalog database, settings schema, or deployed dependencies.

## Decisions

- Add `wit::ui::GeneralSettingsDialog` in `src/ui`, derived from `ATL::CDialogImpl<GeneralSettingsDialog>` and compiled through `WtlSupport.h`. It defines `IDD = IDD_GENERAL_SETTINGS` and handles initialization, `IDOK`, and `IDCANCEL` through a message map. Alternative considered: retain the raw callback in `MainFrame.cpp`, rejected because it preserves the ownership exception this change exists to remove.

- Give the dialog a narrow API such as `bool Show(HWND owner, const wit::platform::AppSettings& current, wit::platform::AppSettings& accepted)`. The dialog maintains an editable copy, fills its controls from `current`, and writes `accepted` only after `IDOK`. Alternative considered: pass `CatalogSession` into the dialog, rejected because presentation should not own settings persistence.

- Reuse `IDD_GENERAL_SETTINGS`, `IDC_SHOW_STATUS_BAR`, and `IDC_LAST_OPENED_CATALOG` in the existing resource file. The change is about code ownership and WTL hosting, and there is no reason to alter native layout or user-visible text. Alternative considered: recreate controls in code, rejected because the application already uses stable native resources.

- Reduce `MainFrame::OnGeneralSettings()` to constructing and showing the UI dialog, calling `session_.SaveSettings()` only on accepted edits, reporting save failures, and updating status-bar visibility after successful persistence. Remove its local dialog-data struct and raw dialog procedure. Alternative considered: let the dialog save `settings.ini` directly, rejected because it would mix presentation with application-state ownership and make error/application update behavior harder to coordinate.

- Register the new source/header pair in the supported MSBuild project. Continue using vendored WTL/MSVC ATL headers already present; no new runtime is introduced. Alternative considered: add a separate dialog framework, rejected because it is unnecessary and conflicts with the native WTL/ATL architecture.

## Risks / Trade-offs

- [Accepted values might be applied when the user cancels] -> Have the dialog publish its edited copy only after an `IDOK` modal result and smoke-test Cancel.
- [Moving initialization could change displayed status or last-catalog text] -> Reuse the existing resource IDs and exact initialization behavior in the WTL handler.
- [Persistence could accidentally move into UI code] -> Keep the result value-based and retain all `CatalogSession::SaveSettings()` handling in `MainFrame`.
- [A compile-only migration could leave an untested dialog route] -> Build the supported x64 configuration and smoke-test opening, confirming, cancelling, persisted reload, and status-bar application.

## Migration Plan

1. Add the dedicated WTL/ATL `GeneralSettingsDialog` files and include them in `WhereIsThat.vcxproj`.
2. Move dialog initialization and accept/cancel message handling from `MainFrame.cpp` into the new dialog component.
3. Update `MainFrame::OnGeneralSettings()` to consume the dialog result while retaining persistence and status-bar refresh responsibilities.
4. Build `Release|x64` and smoke-test the existing General Settings workflows.

Rollback removes the new dialog files and restores the prior inline callback. No setting or catalog data migration is required because the resource identifiers and `settings.ini` contract remain unchanged.

## Open Questions

None block implementation. Additional settings fields should be proposed independently when their behavior is defined.
