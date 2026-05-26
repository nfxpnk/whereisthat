## ADDED Requirements

### Requirement: Reliable fixed-volume disk type classification
The system SHALL attempt native fixed-volume device classification during a new or replacement scan and SHALL persist `HardDisk` or `SolidStateDisk` when Windows reliably reports whether the selected fixed volume incurs a seek penalty.

#### Scenario: Fixed solid-state volume exposes its storage property
- **WHEN** a scanned non-virtual source is on a fixed Windows volume whose native device property reports no seek penalty
- **THEN** the persisted disk type is `SolidStateDisk`

#### Scenario: Fixed rotating volume exposes its storage property
- **WHEN** a scanned non-virtual source is on a fixed Windows volume whose native device property reports a seek penalty
- **THEN** the persisted disk type is `HardDisk`

#### Scenario: Fixed volume cannot be reliably distinguished
- **WHEN** a scanned non-virtual source is on a fixed Windows volume but its seek-penalty property cannot be queried reliably
- **THEN** the persisted disk type remains `Other`

### Requirement: Existing explicit media classifications remain authoritative
The system SHALL preserve existing explicit classification of mounted ISO sources as `VirtualDisk` and removable Windows volumes as `RemovableUSB` when fixed-volume HDD/SSD classification is added.

#### Scenario: Mounted ISO source is scanned
- **WHEN** a user scans a selected ISO through its mounted readable volume
- **THEN** its persisted disk type remains `VirtualDisk`

#### Scenario: Removable source is scanned
- **WHEN** a non-virtual selected source resolves to a Windows removable volume
- **THEN** its persisted disk type is `RemovableUSB`
