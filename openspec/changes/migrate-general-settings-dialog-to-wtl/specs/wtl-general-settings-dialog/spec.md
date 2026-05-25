## ADDED Requirements

### Requirement: Dedicated WTL-hosted General Settings dialog
The system SHALL present `General Settings` through a dedicated native WTL/ATL dialog component in the UI boundary rather than an inline main-frame dialog procedure, while retaining the existing resource-backed native appearance and implemented controls.

#### Scenario: User opens General Settings
- **WHEN** the user invokes `Options > General Settings`
- **THEN** the application opens the existing modal General Settings surface through the dedicated WTL/ATL dialog component
- **THEN** the dialog displays the implemented `Show status bar` preference and read-only last-opened catalog information

### Requirement: General Settings coordination boundary
The system SHALL keep dialog control handling within the dedicated General Settings UI component and keep confirmed preference persistence and main-window display application under main-frame/session coordination.

#### Scenario: User confirms changed General Settings
- **WHEN** the user changes `Show status bar` and confirms the dedicated dialog
- **THEN** the dialog returns the confirmed edited settings to its caller
- **THEN** application coordination persists the confirmed settings through the existing `settings.ini` path and applies status-bar visibility

#### Scenario: User cancels General Settings
- **WHEN** the user changes controls in the dedicated dialog and selects Cancel
- **THEN** the dialog returns without confirmed edited settings
- **THEN** the application does not persist or apply those cancelled changes

### Requirement: WTL migration preserves settings semantics
The system SHALL perform the General Settings WTL dialog migration without changing the existing preference schema, catalog storage, command route, or native dependency model.

#### Scenario: Existing persisted preference is displayed after migration
- **WHEN** `settings.ini` contains the previously supported `General.ShowStatusBar` value and the user opens General Settings
- **THEN** the migrated dialog initializes `Show status bar` from that value under the same established behavior
- **THEN** no catalog database migration or additional UI runtime is required
