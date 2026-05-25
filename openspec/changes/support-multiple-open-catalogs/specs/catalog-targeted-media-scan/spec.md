## ADDED Requirements

### Requirement: Destination catalog selection in Add New Disk/Media
The system SHALL present a catalog-selection control immediately after the `Disk name:` input in the Add New Disk/Media dialog and SHALL use it to identify the open editable catalog that will receive an accepted Add/Update operation.

#### Scenario: Dialog opens for an editable active catalog
- **WHEN** the user invokes Add/Update while an editable catalog is active
- **THEN** the dialog displays the open destination catalog choices immediately after `Disk name:` in its layout and tab order
- **THEN** the active editable catalog is initially selected as the destination

#### Scenario: Multiple editable catalogs are open
- **WHEN** the Add New Disk/Media dialog is shown while multiple editable catalogs are open
- **THEN** the user can choose which open editable catalog will receive the staged scan

#### Scenario: Protected catalog is open
- **WHEN** an open catalog is protected or read-only
- **THEN** the dialog does not allow it to be confirmed as a destination for a catalog modification

#### Scenario: No editable destination is available
- **WHEN** the operation has no open editable destination catalog
- **THEN** the application does not start a scan
- **THEN** the user is informed that an editable catalog must be opened or selected

### Requirement: Destination-aware staged scan delivery
The system SHALL bind each accepted media scan to the selected destination catalog identity and SHALL stage and save changes only for that destination catalog, even when the user's TreeView selection changes while the scan runs.

#### Scenario: User targets a non-active destination
- **WHEN** the user chooses a different open editable catalog in the dialog and confirms a valid media source
- **THEN** the scan is prepared from and staged into the chosen destination catalog's working view
- **THEN** the previously active catalog is not modified solely because it launched the dialog

#### Scenario: Selection changes during a scan
- **WHEN** a scan is running for one destination catalog and the user selects or browses another open catalog
- **THEN** successful completion is adopted into the originally selected destination catalog
- **THEN** the newly viewed catalog does not receive that pending scan result

#### Scenario: Destination already has pending edits
- **WHEN** the selected destination catalog has pending edits before a new scan starts
- **THEN** the new staged scan is based on that destination's current working contents
- **THEN** previously pending edits in that destination are not silently discarded by the accepted scan result

#### Scenario: Destination validation changes before scan starts
- **WHEN** the chosen destination no longer exists or is no longer editable when dialog confirmation is processed
- **THEN** the system does not start a scan for that destination
- **THEN** no other open catalog is used as an implicit fallback destination

### Requirement: Scan activity constraints across open catalogs
The system SHALL retain a single running Add/Update scan at a time and SHALL protect only state transitions that would invalidate the running scan's destination.

#### Scenario: Second scan is requested
- **WHEN** one catalog scan is running and the user requests another Add/Update scan for any catalog
- **THEN** the system does not start a concurrent second scan

#### Scenario: User browses another catalog while scanning
- **WHEN** an Add/Update scan is running for one catalog
- **THEN** the user can browse another open catalog without changing the scan destination

#### Scenario: Target save or discard is requested while scanning
- **WHEN** an Add/Update scan is still producing pending state for a destination catalog
- **THEN** the system does not save, discard, or close that destination's pending state until the scan has been safely resolved
