## ADDED Requirements

### Requirement: Active media scan status dialog
The system SHALL show a scanning status dialog after a valid Add/Update Disk Image request starts a background scan and SHALL dismiss it when that scan completes or is cancelled.

#### Scenario: Accepted media scan begins
- **WHEN** the user confirms a valid Add/Update Disk Image selection and the background scan starts
- **THEN** a scanning status dialog is displayed without blocking asynchronous scan progress processing

#### Scenario: Active media scan finishes
- **WHEN** the active Add/Update Disk Image scan completes, fails, or is cancelled
- **THEN** the scanning status dialog is closed

### Requirement: Scan cancellation
The scanning status dialog SHALL provide a Cancel action that requests cancellation of the active scan and SHALL NOT apply partially scanned staged content after cancellation.

#### Scenario: User cancels an active scan
- **WHEN** the user selects Cancel in the scanning status dialog while the scan is running
- **THEN** the application requests cancellation and indicates that cancellation is pending until the worker completes
- **THEN** no partially scanned staged content is accepted into the destination catalog

### Requirement: Scanned item counts
The scanning status dialog SHALL display cumulative scanned file and folder counts reported by the active scan.

#### Scenario: Items are enumerated
- **WHEN** progress is reported during an active media scan
- **THEN** the dialog displays the reported file count and folder count

### Requirement: Developer test pacing
The scan implementation SHALL provide disabled-by-default compile-time constants to enable a delay, expressed in microseconds, between processed files for progress-dialog testing with small scan sources.

#### Scenario: Test delay is enabled
- **WHEN** a developer enables the scan delay constant and scans files
- **THEN** the worker pauses by the configured microsecond duration between processed files and publishes progress suitable for observing the dialog on a short scan
