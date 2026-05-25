## 1. Dialog Ownership Boundary

- [x] 1.1 Confirm `GeneralSettingsDialog`, `SearchDialog`, and `AddNewDiskMediaDialog` remain application-owned resource dialogs implemented as `ATL::CDialogImpl<T>` classes in dedicated `src/ui` header/source pairs.
- [x] 1.2 Keep native `::MessageBoxW` prompts and feature-local Add New Disk/Media folder/ISO shell picker behavior unchanged, documenting in implementation notes as needed that these are not resource-dialog migrations.
- [x] 1.3 Confirm no implementation, resource, or integration change is made for `ProgressDialog` as part of this work.

## 2. Catalog Shell Picker Component

- [x] 2.1 Add dedicated `src/ui/CatalogFileDialog.h` and `src/ui/CatalogFileDialog.cpp` files that own the catalog create/open Windows shell-picker setup and selected filesystem-path extraction.
- [x] 2.2 Implement the extracted catalog picker behavior with `IFileSaveDialog` and `IFileOpenDialog`, preserving the current titles, `.db` filter/default extension, filesystem/path-existence constraints, no-change-directory option, acceptance results, and cancellation behavior.
- [x] 2.3 Register the catalog picker header/source pair in `WhereIsThat.vcxproj` without adding a new dialog resource, runtime, or library dependency.

## 3. Main Frame Integration

- [x] 3.1 Remove the inline catalog file-picker constants/helpers and COM dialog-launching implementation from `src/app/MainFrame.cpp`.
- [x] 3.2 Update New Catalog and Open Catalog command handling to invoke the dedicated catalog picker UI component while retaining pending-change confirmation, session activation, persistence, and error-reporting responsibilities in `MainFrame`.
- [x] 3.3 Verify no resource-backed dialog procedure or control-state implementation is introduced inline into coordinating application code.

## 4. Verification

- [x] 4.1 Build `whereisthat.sln` for `Release|x64` through MSBuild and verify the new UI component compiles with the existing WTL/ATL and COM shell-dialog dependency set.
- [ ] 4.2 Smoke-test creating and opening a catalog through accepted and cancelled picker flows, confirming their titles/filters and existing catalog workflow results remain unchanged.
- [ ] 4.3 Smoke-test General Settings, Search for Items, Add New Disk/Media (including folder and ISO selection), and relevant native message-box prompts to confirm dialog standardization caused no user-visible regressions and did not introduce progress UI.
