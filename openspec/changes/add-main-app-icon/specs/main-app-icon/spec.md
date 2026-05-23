## ADDED Requirements

### Requirement: Application icon resource
The system SHALL include an application-owned icon resource compiled into the Windows executable.

#### Scenario: Executable exposes app icon
- **WHEN** the application is built
- **THEN** the resulting executable contains the main application icon resource

#### Scenario: Icon asset supports Windows shell sizes
- **WHEN** Windows requests common shell icon sizes
- **THEN** the icon resource provides suitable imagery for large and small icon surfaces

### Requirement: Main window uses app icon
The system SHALL use the main application icon for the primary window instead of the generic Windows application icon.

#### Scenario: Window class registration
- **WHEN** the main window class is registered
- **THEN** the class uses the application icon resource for its large icon
- **THEN** the class uses the application icon resource for its small icon

#### Scenario: User views app in Windows UI
- **WHEN** the application window is shown in the title bar, taskbar, or Alt-Tab switcher
- **THEN** Windows displays the Where Is That? application icon rather than a generic system icon

### Requirement: Existing behavior remains unchanged
The system SHALL preserve existing application startup, menu, scanning, catalog, and list view behavior while adding the icon.

#### Scenario: App launches after icon change
- **WHEN** the user starts the application
- **THEN** the main window opens with the existing title, menu, status bar, catalog list, and file list behavior available
