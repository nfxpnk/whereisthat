## 1. Dialog Resources And UI Boundary

- [x] 1.1 Add native dialog and control resource identifiers/template for `Add New Disk/Media`, including source controls, identity inputs, grouped disk-image options, completion options, bottom actions, and initial status text with resource-order tab navigation and default/cancel buttons.
- [x] 1.2 Implement `src/ui/AddNewDiskMediaDialog` using the established Win32 dialog pattern, add it to the Visual Studio project, and expose a focused result type for selected source kind, paths, and effective disk name.
- [x] 1.3 Populate available local/removable drive-letter controls on initialization and present the requested fixed-size native layout and deferred placeholder controls without assigning unrequested behavior.

## 2. Source Selection And Validation

- [x] 2.1 Implement drive and Network/computer selection using accessible filesystem-root validation, updating dialog source state, disk-name default, bottom status, and `OK` enablement.
- [x] 2.2 Implement ISO file selection with an ISO-filtered native picker and supported inbox Windows resolution/mount handling that yields a readable scan root or reports failure without starting a scan.
- [x] 2.3 Ensure `OK` starts disabled and is enabled only for a usable selected source, while Cancel and deferred Settings/Troubleshooting/options close or remain inert without creating pending changes or altering settings.

## 3. Existing Add/Update Integration

- [x] 3.1 Replace the direct folder-picker branch in `MainFrame::OnAddOrUpdateDiskImage()` with the modal dialog result while retaining existing no-catalog, protected-catalog, and scan-in-progress checks.
- [x] 3.2 Pass accepted resolved roots and the effective new-source name into the existing working-copy, asynchronous scanning, source-refresh, progress, completion, and pending-Save workflow without changing saved catalog semantics.
- [x] 3.3 Confirm menu selection, toolbar command dispatch, and `Ctrl+D` continue to reach the one shared Add/Update route and dialog.

## 4. Documentation And Verification

- [x] 4.1 Update maintained UI/acceptance documentation to describe the Add New Disk/Media entry surface, validation/cancellation behavior, and preserved staged-save path.
- [x] 4.2 Build the supported x64 configuration and fix compile/resource/project-definition errors introduced by the dialog change.
- [ ] 4.3 Smoke-test dialog appearance and tab/default/cancel behavior, disabled/enabled `OK`, supported drive/network and ISO success/failure paths, placeholders, command routes, and new-source versus existing-source pending Add/Update behavior.

## 5. Compact Dialog Refinements

- [x] 5.1 Resize and tighten the dialog to the compact 510-pixel reference, using sequential uniform 42-by-42-pixel source buttons for displayed drives, ISO, and Network/computer selection.
- [x] 5.2 Reduce checkbox row spacing and disable the three archive-dependent options until Browse inside compressed files is checked.
- [x] 5.3 Rebuild and smoke-test the compact dimensions, source-button packing, initial disabled controls, and archive-option enable/disable transition.
