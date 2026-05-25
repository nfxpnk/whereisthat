## ADDED Requirements

### Requirement: Application-owned dialogs use the WTL dialog component pattern
The system SHALL implement each application-owned, resource-backed dialog window as an `ATL::CDialogImpl<T>` UI component with its dialog-specific behavior in dedicated matching header and source files under `src/ui`.

#### Scenario: Existing resource-backed dialogs are maintained
- **WHEN** General Settings, Search for Items, or Add New Disk/Media dialog behavior is built or modified
- **THEN** its resource-backed dialog behavior is owned by its corresponding `CDialogImpl` UI component
- **THEN** its implementation remains separated from main-frame coordination in its dedicated UI files

#### Scenario: A future application-owned resource dialog is added
- **WHEN** a later feature adds an application-owned dialog template with dialog-specific controls or event handling
- **THEN** that feature provides a dedicated `CDialogImpl`-based UI class and matching source/header files rather than implementing the dialog procedure inline in an orchestrating component

### Requirement: Windows-owned modal UI remains native where appropriate
The system SHALL continue to use operating-system dialog APIs for standard messages and shell filesystem selection when no application-owned resource-backed interaction is required, and SHALL not require `CDialogImpl` wrappers solely because those APIs display modal UI.

#### Scenario: Catalog file path is selected
- **WHEN** the user creates a new catalog or opens an existing catalog through a filesystem picker
- **THEN** the application displays the appropriate Windows shell save/open dialog with its current filter, title, path constraints, and cancellation behavior
- **THEN** the shell picker presentation is owned by a dedicated UI component rather than implemented inline by `MainFrame`

#### Scenario: An application workflow displays a short prompt
- **WHEN** an existing workflow needs to report an error, inform the user, or request a simple confirmation
- **THEN** the workflow may continue to display a native message box without introducing a custom `CDialogImpl` resource dialog

#### Scenario: Add New Disk or Media selects a subordinate source
- **WHEN** the Add New Disk/Media dialog invokes its folder or ISO source selection flow
- **THEN** it may use its native shell picker behavior as part of the owning dialog component without creating a separate application-owned dialog template

### Requirement: Progress dialog implementation is deferred
The system SHALL NOT implement or convert a progress dialog as part of the dialog component standardization change.

#### Scenario: Dialog standardization is delivered
- **WHEN** this change is implemented and verified
- **THEN** application-owned dialogs and catalog picker ownership meet the defined boundary
- **THEN** `ProgressDialog` remains deferred for a separately specified implementation
