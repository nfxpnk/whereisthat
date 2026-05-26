## ADDED Requirements

### Requirement: Additional top-level menu order
The system SHALL present `View`, `Search`, `Actions`, `Options`, and `Window` top-level menus immediately after `Edit` in that order, while retaining the Help menu after them.

#### Scenario: User inspects the main menu bar
- **WHEN** the main window menu bar is displayed
- **THEN** the top-level application menus appear in the order `File`, `Edit`, `View`, `Search`, `Actions`, `Options`, `Window`, and `Help`

### Requirement: View menu command surface
The system SHALL present a View menu containing, in order, `Sort items`, `View Icons`, `View List`, `View Small Icons`, `View Details`, `View Thumbnails`, `Columns Setup`, `Show Alias Item Names`, `Locate in Catalog`, `Toolbar`, and `Status Bar`.

#### Scenario: User opens View menu
- **WHEN** the user opens the View menu in the main window
- **THEN** all required View commands are visible in the specified order

#### Scenario: User selects a View placeholder
- **WHEN** the user selects any View command introduced in this change
- **THEN** the application performs no view-layout, catalog, file, or persisted-settings operation for that placeholder command

### Requirement: Search menu command surface
The system SHALL replace the existing Search command surface with, in order, `Search for Items`, `Find in This Catalog`, `Find Selected Items`, `Scan for Duplicates`, `Compare to Media`, `Compare Files to Catalog`, and `Compare Cataloged Data`.

#### Scenario: User opens Search menu
- **WHEN** the user opens the Search menu in the main window
- **THEN** all required Search commands are visible in the specified order
- **THEN** `Search for Items` displays `Ctrl+F` beside its name

#### Scenario: User selects a Search placeholder
- **WHEN** the user selects any Search command introduced in this change or invokes its displayed shortcut
- **THEN** the application performs no search, comparison, scan, catalog, or settings operation for that placeholder command

### Requirement: Actions menu command surface
The system SHALL present an Actions menu containing, in order, `Open in Explorer`, `View File`, `Launch File`, `Edit Description`, `User List`, `Rename Catalog`, `Remove Alias Name`, `File Management`, `Remove from Catalog`, `Remove Archive Contents`, `Remove Thumbnail`, and `Properties`.

#### Scenario: User opens Actions menu
- **WHEN** the user opens the Actions menu in the main window
- **THEN** all required Actions commands are visible in the specified order
- **THEN** `View File` displays `Ctrl+I`, `Edit Description` displays `Ctrl+E`, and `Properties` displays `Ctrl+P` beside their names

#### Scenario: User selects an Actions placeholder
- **WHEN** the user selects any Actions command introduced in this change or invokes its displayed shortcut
- **THEN** the application performs no filesystem, catalog, file, thumbnail, description, or settings operation for that placeholder command

### Requirement: Options menu command surface
The system SHALL present an Options menu containing, in order, `General Settings`, `User Interface Setup`, `File List Settings`, `Disk Image Settings`, and `Description Settings`.

#### Scenario: User opens Options menu
- **WHEN** the user opens the Options menu in the main window
- **THEN** all required Options commands are visible in the specified order

#### Scenario: User selects an Options placeholder
- **WHEN** the user selects `User Interface Setup`, `File List Settings`, `Disk Image Settings`, or `Description Settings`
- **THEN** the application performs no interface, file-list, disk-image, description, catalog, or persisted-settings operation for that placeholder command

### Requirement: Window menu command surface
The system SHALL present a Window menu containing, in order, `Next Window` and `Search Window`.

#### Scenario: User opens Window menu
- **WHEN** the user opens the Window menu in the main window
- **THEN** all required Window commands are visible in the specified order
- **THEN** `Next Window` displays `Ctrl+F6` beside its name

#### Scenario: User selects a Window placeholder
- **WHEN** the user selects `Next Window` or `Search Window`
- **THEN** the application performs no window, catalog, file, or persisted-settings operation for that placeholder command

### Requirement: Help menu command surface
The system SHALL present a Help menu containing, in order, `Help` and `About`.

#### Scenario: User opens Help menu
- **WHEN** the user opens the Help menu in the main window
- **THEN** all required Help commands are visible in the specified order
- **THEN** `Help` displays `F1` beside its name

#### Scenario: User selects the Help placeholder
- **WHEN** the user selects `Help`
- **THEN** the application performs no help-navigation, catalog, file, or persisted-settings operation for that placeholder command
