## Context

The application uses vendored WTL with MSVC ATL. Its three application-owned resource dialogs are already properly separated under `src/ui`: `GeneralSettingsDialog`, `SearchDialog`, and `AddNewDiskMediaDialog` each derive from `ATL::CDialogImpl<T>` and own their message-map behavior in a dedicated header/source pair.

Not every visible modal surface should be a `CDialogImpl` class. `MainFrame.cpp` currently launches Windows shell `IFileSaveDialog` and `IFileOpenDialog` instances for catalog creation and opening, while `AddNewDiskMediaDialog.cpp` launches subordinate shell folder/ISO selectors and both UI areas use `::MessageBoxW` for standard prompts. Shell common dialogs and message boxes are operating-system-owned UI; replacing them with custom resource dialogs would lose expected platform behavior without improving ownership.

The remaining boundary issue is that catalog shell-picker setup and result extraction are local functions in `MainFrame.cpp`, although that frame should coordinate catalog operations rather than implement dialog presentation. `ProgressDialog` is a currently deferred state-only placeholder and is explicitly outside this work.

## Goals / Non-Goals

**Goals:**
- Record and preserve `ATL::CDialogImpl<T>` plus dedicated `.h`/`.cpp` files as the convention for application-owned, resource-backed dialogs.
- Extract the catalog create/open shell-picker presentation from `MainFrame.cpp` into a dedicated `src/ui` component with its own header/source files.
- Preserve each existing dialog workflow, native platform behavior, ownership relationship, command route, and returned catalog path.
- Keep the scope clear that `ProgressDialog` will be implemented in a later change.

**Non-Goals:**
- Replace `IFileOpenDialog`, `IFileSaveDialog`, or `::MessageBoxW` with custom dialog templates.
- Redesign General Settings, Search, Add New Disk/Media, catalog selection, or any message text/layout.
- Extract private folder/ISO chooser details out of `AddNewDiskMediaDialog` merely because they display subordinate Windows-owned picker windows.
- Implement a progress window, progress resource, progress command flow, or scan progress presentation.
- Change persisted settings, catalog database behavior, scan behavior, resources, or dependencies.

## Decisions

- Treat an **application-owned dialog** as a resource-backed window whose controls and event handling are defined by this application. Such dialogs SHALL use an `ATL::CDialogImpl<T>` class in a matching `src/ui/<DialogName>.h` and `.cpp` pair. The current `GeneralSettingsDialog`, `SearchDialog`, and `AddNewDiskMediaDialog` already satisfy this decision and require verification rather than conversion. Alternative considered: rewrite or rename the existing conforming dialogs, rejected because it adds churn without changing the boundary.

- Do not classify Windows common dialogs or standard message boxes as missing `CDialogImpl` migrations. `IFileOpenDialog` / `IFileSaveDialog` provide native file-system picker behavior, and `::MessageBoxW` is appropriate for short alerts and confirmations. Alternative considered: create application resource templates for every modal window seen by the user, rejected because it duplicates operating-system UI and misapplies `CDialogImpl`.

- Add a focused UI component such as `CatalogFileDialog.h` and `CatalogFileDialog.cpp` to own catalog create/open shell-dialog configuration and filesystem-path result extraction currently implemented by `ChooseNewCatalogPath` and `ChooseCatalogToOpen` in `MainFrame.cpp`. It exposes narrow operations for selecting a new catalog destination and selecting an existing catalog; internally it remains based on `IFileSaveDialog` and `IFileOpenDialog`, not `CDialogImpl`. Alternative considered: keep these helper functions in `MainFrame`, rejected because the frame should not own dialog presentation configuration.

- Keep the folder and ISO Windows pickers used by `AddNewDiskMediaDialog` local to that dialog component. They are subordinate source-selection mechanics of one already separated application-owned dialog rather than independent command-level dialogs. Alternative considered: produce separate classes/files for every shell picker invocation, rejected because it fragments a cohesive feature and provides no reusable behavior.

- Leave simple `MessageBoxW` prompts at their coordinating or owning UI call sites. They contain no control lifecycle or resource-backed interaction deserving a separate dialog component. Alternative considered: centralize all message boxes, rejected because it obscures workflow-specific messages without addressing the stated WTL standard.

- Leave `src/ui/ProgressDialog.h` and `.cpp` untouched except for ensuring this change does not accidentally rely on them. A later proposal can define its resource, cancellation behavior, scan binding, and ownership. Alternative considered: add an empty `CDialogImpl` shell now, rejected because the user has explicitly deferred the progress dialog.

## Risks / Trade-offs

- [The standardization could be mistaken for a request to custom-build shell pickers] -> State explicitly in the spec and code-review criteria that Windows-owned common dialogs remain native.
- [Extracting catalog picker helpers could change filters, options, titles, or cancellation behavior] -> Move their configuration and path-extraction behavior unchanged and smoke-test both create and open paths plus cancellation.
- [A broad "separate files" rule could split subordinate prompts into excessive components] -> Apply separate dialog components to resource-backed application dialogs and command-level shell-picker ownership, while leaving feature-local Windows prompts with their owner.
- [Progress UI could accidentally be included during cleanup] -> Include an explicit excluded-scope task and verify no `ProgressDialog` implementation changes are part of the change.

## Migration Plan

1. Confirm each existing resource-backed dialog remains implemented as a `CDialogImpl` class in its dedicated `src/ui` source/header pair.
2. Add the catalog shell-picker UI component and register it in the supported MSBuild project.
3. Remove the inline catalog picker helpers from `MainFrame.cpp` and have the catalog commands invoke the UI component while retaining catalog/session coordination in `MainFrame`.
4. Build `Release|x64` and smoke-test custom dialog launch routes, catalog create/open picker acceptance and cancellation, relevant prompts, and Add New Disk/Media subordinate pickers.
5. Confirm that `ProgressDialog` has not been implemented or extended in this change.

Rollback restores the catalog-picker helper functions to `MainFrame.cpp` and removes the new UI files. No resource, settings, or catalog data migration is involved.

## Open Questions

None block implementation. The catalog shell-picker component may use static operations or an instance interface according to the surrounding coding style, provided the ownership and unchanged native behavior remain clear.
