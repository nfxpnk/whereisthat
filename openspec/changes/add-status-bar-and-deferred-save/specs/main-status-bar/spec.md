## ADDED Requirements

### Requirement: Five-section main status bar
The system SHALL divide the visible bottom status bar of the main window into, from left to right, a catalog state section, a narrow catalog protection section, a focused-item details section, a selected-items summary section, and a program-status lights section, and SHALL resize those sections with the main window.

#### Scenario: Status bar is displayed
- **WHEN** the main window is displayed with status-bar visibility enabled
- **THEN** the five logical sections are presented in the required left-to-right order
- **THEN** focused-item and selected-items text sections use available resizing space while icon-oriented sections remain compact

#### Scenario: Main window is resized
- **WHEN** the user resizes the main window while the status bar is visible
- **THEN** the status bar remains at the bottom of the client area
- **THEN** its five sections are recalculated without overlapping the browsing surface

### Requirement: Catalog state and protection indicators
The system SHALL display `Loaded` for an active catalog with no pending modifications, `Modified` for an active catalog with pending modifications, and a lock indicator in the narrow protection section when the active catalog is read-only, locked, or protected.

#### Scenario: Editable catalog is loaded
- **WHEN** an editable active catalog is opened or saved successfully with no pending modifications
- **THEN** the catalog state section displays `Loaded`
- **THEN** the protection section does not display a locked-state icon

#### Scenario: Active catalog has pending changes
- **WHEN** an edit has been successfully staged for the active catalog but not saved
- **THEN** the catalog state section displays `Modified`

#### Scenario: Protected catalog is active
- **WHEN** a supported catalog is active but cannot be edited because it is read-only, locked, or protected
- **THEN** the catalog protection section displays a lock icon or equivalent compact locked indicator

### Requirement: Focused item detail display
The system SHALL display compact filename, size, and stored date details for the item currently focused in the main contents view and SHALL refresh that display when focus or location contents changes.

#### Scenario: File receives focus
- **WHEN** a file item becomes focused in the main contents view
- **THEN** the focused-item section shows its filename, human-readable size, and stored date in a compact status-bar format

#### Scenario: No item is focused
- **WHEN** the contents view is empty or does not have a focused item
- **THEN** the focused-item section is empty or displays a neutral no-item value without stale details from the prior focus

### Requirement: Selected item summary display
The system SHALL support selecting multiple items in the main contents view and SHALL display the number and human-readable aggregate size of the currently selected items using the format `Selected item(s): {totalselected} (total: {human readable total size of selected items})`.

#### Scenario: Multiple items are selected
- **WHEN** the user selects three contents items whose combined size formats as `14.2 MB`
- **THEN** the selected-items section displays `Selected item(s): 3 (total: 14.2 MB)`

#### Scenario: Selection changes
- **WHEN** items are selected or deselected in the main contents view
- **THEN** the selected-items section updates to reflect the current selection count and combined stored size

### Requirement: Extensible program status lights
The system SHALL provide an application-wide status state supporting at least `Idle`, `Busy`, and `Searching`, SHALL route status-light presentation through one update boundary, and for this change SHALL display grey and green placeholder status lights in the rightmost status section.

#### Scenario: Initial placeholder lights are displayed
- **WHEN** the main status bar is shown under this implementation
- **THEN** its rightmost section displays grey and green placeholder lights/icons

#### Scenario: Application status changes
- **WHEN** the application status transitions among `Idle`, `Busy`, or `Searching`
- **THEN** the status-light update boundary is invoked so future state-specific icon behavior can be introduced without restructuring the status bar
