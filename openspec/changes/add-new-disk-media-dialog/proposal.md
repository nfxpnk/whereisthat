## Why

`Edit > Add/Update Disk Image` currently jumps directly to a generic folder picker, so it cannot present the media-selection and scan-option workflow expected by the desktop application. A dedicated native Add New Disk/Media dialog gives this command the correct entry surface while continuing to use the implemented staged scan/update path.

## What Changes

- Replace the current Add/Update command's direct folder-picker entry point with a modal native dialog titled `Add New Disk/Media`, reached from menu, toolbar, and `Ctrl+D` dispatch through the existing command identifier.
- Present selectable local/removable drive letters, an ISO-source action, and a network/computer source action across the dialog's top row, along with media number and disk name inputs.
- Present the requested update-mode, folder-limitations, active-plugins, disk-image-settings, completion-options, settings, confirmation, cancellation, troubleshooting, and status controls, leaving explicitly deferred controls as non-functional placeholders.
- Require selection of a valid media source or ISO path before enabling `OK`; canceling closes the dialog without starting a scan or modifying pending catalog state.
- Feed an accepted, supported media selection into the existing asynchronous add/update and staged-save workflow without changing catalog persistence semantics.

## Capabilities

### New Capabilities
- `add-new-disk-media-dialog`: Defines the Add New Disk/Media modal UI, source validation and confirmation behavior, placeholders, and handoff to existing add/update scanning.

### Modified Capabilities

## Impact

- Native UI/resources under `src/ui` and `src/app` are expected to gain the dialog implementation, resource/control identifiers, and any required source-button imagery following the existing Win32/Common Controls conventions.
- `MainFrame::OnAddOrUpdateDiskImage()` and the `ID_EDIT_ADDDISKIMAGE` command route will invoke the dialog before entering the existing background scan/update orchestration.
- Existing active-catalog checks, protected-catalog behavior, staged changes, Save behavior, scan-progress handling, and database/storage formats remain unchanged.
- No third-party dependency, schema migration, or unrelated menu/toolbar behavior change is introduced.
