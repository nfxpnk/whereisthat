## ADDED Requirements

### Requirement: Add New Disk/Media dialog invocation
The system SHALL invoke a modal native dialog titled `Add New Disk/Media` through the existing Add/Update media command route without changing its active-catalog eligibility and concurrent-scan protections.

#### Scenario: User invokes Add/Update from the menu
- **WHEN** the user selects `Edit > Add/Update Disk Image` while an editable catalog is active and no scan is running
- **THEN** the application displays the modal `Add New Disk/Media` dialog

#### Scenario: User invokes Add/Update through an existing alternate route
- **WHEN** the user activates the shared Add/Update toolbar command or presses `Ctrl+D` while the operation is permitted
- **THEN** the application displays the same modal `Add New Disk/Media` dialog

#### Scenario: Existing operation guard rejects dialog entry
- **WHEN** Add/Update is requested without an editable active catalog or while a scan is running
- **THEN** the application retains its existing informational/protection behavior
- **THEN** the application does not start a new scan through the dialog

### Requirement: Media source and identity controls
The dialog SHALL present source-selection controls for available local/removable drive letters, ISO image selection, and network/computer filesystem selection, and SHALL present `#:` and `Disk name:` text inputs.

#### Scenario: Dialog first appears
- **WHEN** the Add New Disk/Media dialog is initialized
- **THEN** it shows available supported local/removable drive choices as drive-letter source controls
- **THEN** it shows ISO and Network/computer source actions
- **THEN** it shows text inputs labeled `#:` and `Disk name:`

#### Scenario: User chooses an available drive
- **WHEN** the user selects a displayed local or removable drive source
- **THEN** the selected drive root becomes the candidate media source for scanning
- **THEN** the dialog reflects that a drive has been selected

#### Scenario: User chooses a network or computer source
- **WHEN** the user activates the Network/computer source action and accepts an accessible filesystem location through the native selection workflow
- **THEN** that accepted location becomes the candidate media source for scanning

#### Scenario: User chooses an ISO image
- **WHEN** the user activates the ISO action and selects a valid ISO image path
- **THEN** the dialog records that image as the candidate ISO media source
- **THEN** confirmation can proceed only through a supported native resolution to a readable scan source

### Requirement: Dialog layout and deferred control presentation
The dialog SHALL display the requested disk-image option and action layout using native controls, while controls whose operational behavior is deferred SHALL NOT alter catalog contents, scan behavior, application settings, or plugins in this change.

#### Scenario: User views the media dialog
- **WHEN** the dialog is visible
- **THEN** it is presented in a compact approximately 510-pixel-wide native layout with uniform 42-by-42-pixel sequential source buttons for drives, ISO, and Network/computer selection
- **THEN** it displays Update Mode, Folder limitations, and Active Plugins controls
- **THEN** it displays a `Disk Image Settings` group containing the archive browsing and description options, CRC option, existing-description import option, and scan-complete media-description option
- **THEN** it displays the eject, auto-save, and re-display completion options
- **THEN** it displays Settings, OK, Cancel, and Troubleshooting buttons

#### Scenario: Archive browsing is disabled
- **WHEN** `Browse inside compressed files (ZIP, ARJ, RAR, 7Z...)` is not selected
- **THEN** the three dependent archive-description and unchanged-archive options are disabled

#### Scenario: Archive browsing is enabled
- **WHEN** the user selects `Browse inside compressed files (ZIP, ARJ, RAR, 7Z...)`
- **THEN** the three dependent archive-description and unchanged-archive options are enabled for input

#### Scenario: User interacts with a deferred control
- **WHEN** the user activates Update Mode, Folder limitations, Active Plugins, any disk-image or completion option, Settings, or Troubleshooting before its behavior is implemented
- **THEN** the control does not change catalog content, initiate a scan, persist a preference, load a plugin, or invoke a nonexistent help workflow

### Requirement: Validated dialog confirmation
The dialog SHALL keep `OK` disabled until a valid media source or valid ISO selection has been made, SHALL communicate the unselected initial state as `No drive selected`, and SHALL close without changes when canceled.

#### Scenario: No media source has been selected
- **WHEN** the dialog is first opened or contains no valid selected source
- **THEN** the bottom-left status text displays `No drive selected`
- **THEN** the `OK` button is disabled

#### Scenario: Valid drive or network source is selected
- **WHEN** a user selects a readable supported drive root or accepts a valid accessible network/computer filesystem source
- **THEN** the `OK` button is enabled

#### Scenario: Valid ISO source is selected
- **WHEN** the user accepts a valid ISO path that the application can resolve through supported native behavior to a readable scan root
- **THEN** the `OK` button is enabled for that ISO source

#### Scenario: User cancels the dialog
- **WHEN** the user selects Cancel or otherwise dismisses the modal dialog without confirmation
- **THEN** the dialog closes
- **THEN** no scan begins and no pending catalog change is introduced

### Requirement: Accepted media scan handoff
The system SHALL pass an accepted, supported media source from the dialog into the existing asynchronous Add/Update scanning and pending-catalog workflow, preserving current source-update identity and Save semantics.

#### Scenario: User confirms a supported new source
- **WHEN** the user selects a valid supported media source and confirms the dialog
- **THEN** the application scans the resolved source through its existing background Add/Update path
- **THEN** a successful scan stages the newly indexed source in the editable active catalog session according to existing pending-save behavior

#### Scenario: User confirms an existing source
- **WHEN** the accepted dialog source resolves to a media source already indexed in the active catalog
- **THEN** successful scanning stages a refresh of that source without leaving duplicate source contents

#### Scenario: ISO cannot be resolved for scanning
- **WHEN** an accepted ISO image cannot be resolved through supported native behavior to a readable scan source
- **THEN** the application does not start the existing scan workflow for that source
- **THEN** the application informs the user without modifying pending or saved catalog contents
