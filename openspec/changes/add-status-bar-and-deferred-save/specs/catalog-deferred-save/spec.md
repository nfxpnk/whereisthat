## ADDED Requirements

### Requirement: Pending catalog edit state
The system SHALL keep user-requested catalog content modifications in pending state associated with the active catalog until explicit Save and SHALL mark that catalog as modified after a pending edit is successfully staged.

#### Scenario: Add or update scan completes successfully
- **WHEN** `Add/Update Disk Image` completes a scan for an editable active catalog
- **THEN** the scanned addition or replacement is retained as pending catalog content
- **THEN** the selected real catalog database does not yet contain that new or replacement content
- **THEN** the active catalog is marked `Modified`

#### Scenario: Staged replacement is browsed
- **WHEN** a pending Add/Update operation replaces an indexed source in the active catalog
- **THEN** main-window browsing presents the pending replacement content as the current working catalog view
- **THEN** unchanged catalog contents remain available in that view

#### Scenario: Scan fails before staging succeeds
- **WHEN** an Add/Update scan fails without successfully producing a pending edit
- **THEN** the real catalog database remains unchanged
- **THEN** the operation does not newly mark the catalog as modified

### Requirement: Explicit save commits pending changes
The system SHALL make `File > Save`, its accelerator, and its routed toolbar action commit all pending edits for the active editable catalog to its real database atomically and SHALL clear dirty state only after a successful save.

#### Scenario: Modified catalog is saved successfully
- **WHEN** the user invokes Save for an active editable catalog with pending edits and persistence succeeds
- **THEN** all pending catalog edits are committed to the active real catalog database
- **THEN** the pending edit state is cleared
- **THEN** the catalog state returns to `Loaded`

#### Scenario: Save fails
- **WHEN** the user invokes Save for an active catalog with pending edits and persistence fails
- **THEN** no partial set of pending edits is committed to the real catalog database
- **THEN** the pending edits remain available in the working catalog view
- **THEN** the catalog remains marked `Modified`

#### Scenario: Save is invoked without pending changes
- **WHEN** Save is invoked for an active catalog with no pending edits
- **THEN** the catalog remains `Loaded`
- **THEN** no catalog content is changed solely because Save was invoked

### Requirement: Protected catalogs reject modifications
The system SHALL allow a supported protected/read-only catalog to be browsed, SHALL identify it as protected for status display, and SHALL prevent pending edits or Save from attempting to commit changes to it.

#### Scenario: Read-only catalog is opened
- **WHEN** the user opens a supported catalog database that is readable but not editable
- **THEN** it may become the active browsable catalog
- **THEN** it is marked protected for the catalog lock status

#### Scenario: Edit is attempted for protected catalog
- **WHEN** the user attempts Add/Update or Save while the active catalog is protected
- **THEN** the system does not stage or persist catalog modifications
- **THEN** the user is informed that the catalog cannot be edited

### Requirement: Unsaved-change lifecycle guard
The system SHALL prompt the user to Save, Discard, or Cancel before replacing or closing an active catalog session that contains pending edits, unless an equivalent unsaved-change resolution is already in progress.

#### Scenario: User saves before switching catalog
- **WHEN** the user requests New Catalog, Open, or Open Recent while the current catalog is modified and chooses Save
- **THEN** the requested switch proceeds only if saving pending changes succeeds
- **THEN** a failed save preserves the current modified catalog session

#### Scenario: User discards before switching catalog
- **WHEN** the user requests a catalog switch while the current catalog is modified and chooses Discard
- **THEN** pending edits are removed without modifying the real current catalog database
- **THEN** the requested catalog switch may proceed

#### Scenario: User cancels application close
- **WHEN** the user attempts to close or exit while the active catalog is modified and chooses Cancel
- **THEN** the application remains open with the pending edits intact
