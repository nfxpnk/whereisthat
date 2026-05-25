## Why

General Settings is currently implemented as an inline raw `DialogBoxParamW` callback in `MainFrame.cpp`, even though the main window and the other active modal dialogs use WTL/ATL message-map infrastructure. Moving this dialog into its own WTL-based UI component completes the intended dialog ownership boundary and removes settings presentation details from the coordinating frame.

## What Changes

- Add a dedicated `GeneralSettingsDialog` implementation under `src/ui` and move General Settings control initialization, accepted-value collection, and modal dialog dispatch out of `MainFrame.cpp`.
- Implement the dialog through the existing WTL/ATL native dialog pattern (`ATL::CDialogImpl` with a message map), using the existing resource template and controls unless a resource adjustment is required for integration.
- Keep `MainFrame` responsible only for launching the dialog, persisting confirmed settings through `CatalogSession`, and applying changed status-bar visibility.
- Preserve existing user behavior: `Show status bar` initialization and confirmation, read-only last-opened catalog display, Cancel behavior, `settings.ini` persistence, command routing, and native appearance.

## Capabilities

### New Capabilities
- `wtl-general-settings-dialog`: Defines dedicated WTL/ATL-hosted General Settings dialog presentation and its boundary with main-frame coordination and settings persistence.

### Modified Capabilities

None. Existing General Settings preferences and persistence behavior remain unchanged; this change replaces its native dialog implementation structure.

## Impact

- Adds `src/ui/GeneralSettingsDialog.h` and `src/ui/GeneralSettingsDialog.cpp` and registers them in `WhereIsThat.vcxproj`.
- Updates `src/app/MainFrame.cpp` to remove the raw dialog procedure/data plumbing and invoke the dedicated dialog component.
- Continues using existing `src/app/app.rc` and `src/app/resource.h` General Settings resources, `src/platform/AppSettings.*`, `CatalogSession`, and vendored WTL/MSVC ATL support.
- Adds no persisted-data migration, runtime dependency, menu behavior change, or catalog workflow change.
