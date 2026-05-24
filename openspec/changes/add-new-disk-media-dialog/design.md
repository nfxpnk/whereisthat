## Context

The application is a Unicode pure-Win32 desktop executable. `src/app/app.rc` owns dialog/menu resources and `MainFrame::OnCommand()` routes `ID_EDIT_ADDDISKIMAGE`, including toolbar dispatch and the `Ctrl+D` accelerator, into `MainFrame::OnAddOrUpdateDiskImage()`. That handler currently applies scan-in-progress, active-catalog, and editability guards; opens an `IFileOpenDialog` folder picker; and passes the selected normalized root into the implemented background scan and pending-catalog staging path.

There is no equivalent Add New Disk/Media dialog resource or UI class in the present project. Dialog presentation convention is split between resources in `src/app` and dialog behavior in `src/ui` (for example `SearchDialog`), while orchestration and working-catalog state remain owned by `MainFrame`.

The new surface must look and behave like a compact native Windows dialog, must not introduce a new UI toolkit or persistence contract, and must leave existing staged scanning and explicit Save behavior intact. The current scanner enumerates filesystem roots; an ISO file therefore needs native resolution to a readable scan root before it can use that scanner path.

## Goals / Non-Goals

**Goals:**
- Open a modal `Add New Disk/Media` dialog whenever the established Add/Update command is invoked after its existing eligibility checks.
- Provide the requested source selection, media identity, settings/options, confirmation, cancellation, troubleshooting, and bottom-status layout using Win32 dialog resources and project conventions.
- Validate selection so `OK` is disabled until a local/removable drive, accessible network/folder root, or valid ISO selection has produced or can produce a supported source.
- Return a small accepted source result to `MainFrame` and let the existing asynchronous staged-scan/update flow consume its resolved root and display name.
- Keep deferred controls visibly present without assigning undocumented catalog, scanning, settings, or plugin behavior.

**Non-Goals:**
- Implement update policy, folder-limitation, plugin, disk-image-option, completion-option, or Settings persistence/behavior in this change.
- Implement a new archive crawler, CRC computation, media-description editor, eject behavior, or troubleshooting subsystem.
- Change pending edit/Save semantics, active-catalog protection, database schema, scan threading, scan progress, or browsing behavior.
- Add a third-party ISO parsing/mounting library or any non-Win32 UI framework.

## Decisions

- Add a resource-backed dialog, implemented as an `AddNewDiskMediaDialog` UI class under `src/ui` with resources and control IDs under `src/app`, matching the existing `SearchDialog` and native dialog ownership pattern. Its result structure carries source kind, original selected path, resolved filesystem scan root, and effective disk name back to `MainFrame`. Alternative considered: build all dialog handling inline in `MainFrame.cpp`, rejected because the dialog has enough state and control behavior to belong in the established UI boundary.

- Keep the existing `ID_EDIT_ADDDISKIMAGE` route and eligibility guards in `MainFrame`. Replace only the direct `IFileOpenDialog` portion with `AddNewDiskMediaDialog::Show`; after `IDOK`, reuse source normalization, working-copy creation, worker-thread scan, completion messages, and pending-save handling. Toolbar invocation and `Ctrl+D` therefore require no separate wiring. Alternative considered: add a new command ID for the dialog, rejected because it would split a single user action from its existing routes.

- Define the dialog as a fixed-size `DIALOGEX` with `DS_MODALFRAME`, `WS_CAPTION`, `WS_SYSMENU`, Segoe UI font, resource-order tab traversal, `OK` as `DEFPUSHBUTTON`, and `Cancel` mapped to `IDCANCEL`. The top row is populated with native push/check buttons or image buttons for discovered fixed/removable drive letters and dedicated ISO and Network actions; grouped settings and bottom options use standard controls. Alternative considered: a resizable custom window, rejected because this compact settings-and-confirmation surface has no expanding content and existing dialogs are fixed-size modal resources.

- Discover mounted local and removable drive choices using Windows drive enumeration and drive-type APIs when the dialog initializes; selecting a displayed drive yields its root directly. The Network/computer action reuses a native filesystem folder selection suitable for a UNC or mounted/network folder and records the accepted accessible root. Alternative considered: persist a static set of drive-letter controls, rejected because availability changes between machines and while the app is running.

- Make the ISO action open a native ISO-filtered file selection dialog and validate the chosen file. Before accepting into the existing `FileScanner::ScanFolder` path, resolve the ISO selection through supported inbox Windows behavior into a readable filesystem root, recording the ISO as source identity where needed; if native resolution is cancelled or cannot produce a scan root, report the issue and do not start a scan. Alternative considered: pass an `.iso` file path to `ScanFolder`, rejected because it is not a directory and would violate the existing scanner contract; adding a third-party ISO reader is outside scope.

- Keep `OK` disabled at initialization and after an invalidated selection; enable it only after a selectable drive/network root is validated or an ISO path is accepted for native resolution. Set bottom-left status to `No drive selected` until a choice is selected, then update it with selection/resolution status. `Cancel` returns no result and does not create a working copy or launch a worker. Alternative considered: validate only after `OK`, rejected because the disabled-button behavior is a direct acceptance condition.

- Present `#:` as a media-number input and `Disk name:` as a name input. Populate the name from the selected source using the existing source-name convention, allow user editing, and use a nonblank accepted disk name when creating a newly indexed media-source record; existing-source update identity remains its normalized/resolved source path. The number control is captured only as dialog state until a storage/product requirement defines persistence. Alternative considered: add a media-number database column during a UI task, rejected because no persistence behavior is requested and it would expand the catalog contract.

- Render Update Mode, Folder limitations, Active Plugins, Disk Image Settings option checkboxes, completion checkboxes, Settings, and Troubleshooting in the requested layout. Leave them non-operative or disabled where they imply deferred behavior; because no troubleshooting/help flow currently exists beyond About, Troubleshooting is a placeholder in this change. Alternative considered: silently connect these controls to settings or scanning flags, rejected because such semantics have not been specified and could change catalog outcomes.

## Risks / Trade-offs

- `[ISO files are not filesystem roots accepted by the current scanner]` -> Resolve or mount through inbox Windows behavior before launching the existing scanner, and show a non-destructive error if a readable root cannot be obtained.
- `[The screenshot contains controls whose future behavior is not yet specified]` -> Provide their native layout and visible placeholder state without applying settings or scan options.
- `[Media number has no established storage representation]` -> Keep the input in dialog state only and avoid schema churn until persistence requirements are defined.
- `[Dynamic drive count may not fit a fixed screenshot-sized row]` -> Use a bounded source-button row consistent with available dialog width and a native selection fallback for additional/network sources.
- `[Dialog changes could accidentally bypass staged-save or protection rules]` -> Leave guards and post-selection worker/staging code in `MainFrame`, and verify both supported selection and cancellation cases.

## Migration Plan

Add resource/control IDs and the fixed modal dialog template, implement its UI class and source-result handoff, add the class to the MSBuild project, then replace the current folder-picker call within `OnAddOrUpdateDiskImage()` with the accepted-result branch. Update persistent UI/acceptance documentation if maintained alongside implementation, build x64 Release, and smoke-test command routes and staged scans. Rollback removes the dialog class/resources and restores the direct native folder picker without any database migration.

## Open Questions

- Should a later media-metadata change persist the `#:` value, and if so is it unique per catalog or simply a display label?
- Which inbox Windows ISO resolution behavior is acceptable for first delivery on the supported Windows 10/11 versions if mounting requires user or shell interaction?
