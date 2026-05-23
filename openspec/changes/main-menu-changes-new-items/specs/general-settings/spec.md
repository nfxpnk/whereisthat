## ADDED Requirements

### Requirement: General Settings dialog access
The system SHALL provide a native modal General Settings dialog through `Options > General Settings`.

#### Scenario: User opens General Settings
- **WHEN** the user selects `Options > General Settings`
- **THEN** the application displays a native General Settings dialog containing a `Show status bar` preference and confirmation and cancellation actions

### Requirement: INI-backed application preferences
The system SHALL store preference values managed by the General Settings dialog in a local file named `settings.ini`, separate from the catalog database. The initial `Show status bar` preference SHALL default to enabled and SHALL persist as `General.ShowStatusBar`.

#### Scenario: User saves General Settings
- **WHEN** the user changes one or more available General Settings preferences and confirms the dialog
- **THEN** the application writes the confirmed preference values to `settings.ini`
- **THEN** the application does not write those preferences to `catalog.db`

#### Scenario: Saved settings are loaded
- **WHEN** the application opens General Settings after values were previously saved in `settings.ini`
- **THEN** the dialog displays the persisted `Show status bar` value
- **THEN** the main window status bar visibility reflects that persisted value

#### Scenario: Settings file does not yet exist
- **WHEN** the application opens General Settings before `settings.ini` has been created
- **THEN** the dialog presents `Show status bar` as enabled without reporting an error

#### Scenario: User cancels edited settings
- **WHEN** the user changes preference controls in General Settings and cancels the dialog
- **THEN** the application does not persist the unconfirmed changes to `settings.ini`

### Requirement: Placeholder isolation
The system SHALL write `settings.ini` only for implemented settings behavior in this change and SHALL NOT make the placeholder menu commands implicitly alter persisted preferences.

#### Scenario: User invokes a placeholder command
- **WHEN** the user selects any newly listed menu item other than `General Settings`
- **THEN** the application's persisted General Settings values remain unchanged
