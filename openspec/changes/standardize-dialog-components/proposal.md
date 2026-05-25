## Why

The application now has a clean WTL pattern for its application-owned dialogs, but the boundary is implicit and catalog open/create shell-dialog presentation is still embedded in `MainFrame.cpp`. Establishing the dialog ownership rule and extracting that remaining presentation responsibility keeps frame coordination focused without replacing appropriate operating-system dialogs with unnecessary custom UI.

## What Changes

- Establish `ATL::CDialogImpl` in dedicated `src/ui/<DialogName>.h` and `.cpp` files as the standard for application-owned, resource-backed dialog windows.
- Retain the existing `GeneralSettingsDialog`, `SearchDialog`, and `AddNewDiskMediaDialog` implementations as conforming dialog components, with no user-visible redesign required.
- Move catalog database open/create file-picker presentation out of `MainFrame.cpp` into a dedicated UI component in separate source/header files, while continuing to use `IFileOpenDialog` and `IFileSaveDialog` because those are Windows-owned common dialogs.
- Retain `::MessageBoxW` for simple system notifications and confirmation prompts, rather than converting them into resource-backed `CDialogImpl` classes.
- Keep Add New Disk/Media-owned folder and ISO pickers within that separated dialog feature, unless implementation reveals reusable picker behavior worth extracting.
- Explicitly defer `ProgressDialog`; this change does not implement, redesign, or convert progress UI.

## Capabilities

### New Capabilities
- `dialog-component-ownership`: Defines which dialog experiences are app-owned WTL dialog components, how they are separated into UI files, and which Windows-owned prompts/pickers remain native.

### Modified Capabilities

None. This change preserves existing user-visible dialog workflows and introduces an implementation boundary rather than new interaction requirements.

## Impact

- Updates the UI ownership boundary in `src/ui` and removes catalog picker implementation detail from `src/app/MainFrame.cpp`.
- Adds dedicated source/header files for catalog file-picker presentation and registers them in `WhereIsThat.vcxproj`.
- Retains existing dialog resources in `src/app/app.rc` and `src/app/resource.h`, existing WTL/ATL dependencies, COM shell-dialog use, message-box behavior, settings behavior, catalog behavior, and scan behavior.
- Does not implement `src/ui/ProgressDialog.*` beyond leaving its current deferred state untouched.
