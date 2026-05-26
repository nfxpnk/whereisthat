## ADDED Requirements

### Requirement: Catalog workflows have a dedicated decision owner
The system SHALL provide a main-window workflow controller that owns `CatalogSession` and `ScanCoordinator` and SHALL make all business decisions for catalog activation, catalog save, pending-change confirmation, catalog close, Add/Update media scanning, and item-search initiation through that controller.

#### Scenario: Workflow responsibility is extracted
- **WHEN** the workflow-controller refactor is implemented
- **THEN** `ActivateCatalog`, `OnSaveCatalog`, `CloseCatalog`, `ConfirmPendingChanges`, `OnAddOrUpdateDiskImage`, and `OnSearchForItems` are no longer implemented as business-logic methods of `MainFrame`
- **THEN** the controller owns the session and scan-coordination state those workflows consult or modify

#### Scenario: A workflow needs related policy state
- **WHEN** a catalog or scan workflow evaluates active catalog identity, editability, pending state, scan targeting, completion validity, or command eligibility
- **THEN** that evaluation occurs within the controller rather than in `MainFrame`

### Requirement: MainFrame is a reactive UI shell
`MainFrame` SHALL receive native input, forward workflow inputs and UI responses to the controller, and apply typed presentation or refresh effects returned by the controller; it SHALL NOT select workflow outcomes by inspecting catalog-session or scan-coordination state.

#### Scenario: User invokes a catalog command
- **WHEN** a native command invokes activation, Save, Close, Add/Update, or Search behavior
- **THEN** `MainFrame` forwards the relevant intent or UI-supplied input to the controller
- **THEN** the frame updates dialogs, messages, browser presentation, chrome presentation, or window lifetime only as directed by the returned effects

#### Scenario: UI data is required to continue a workflow
- **WHEN** a workflow requires a file/dialog selection or a Save, Discard, Cancel, or close-confirmation answer
- **THEN** the controller requests that input as an effect
- **THEN** `MainFrame` presents the requested UI and passes the answer back without deciding the subsequent catalog action

#### Scenario: Catalog command presentation is refreshed
- **WHEN** catalog selection, pending state, scan state, or catalog membership changes
- **THEN** command/status/recent-menu refresh values used by the frame are supplied by the controller
- **THEN** the frame does not recompute enablement or status from session or scan internals

### Requirement: Controller delegation preserves workflow behavior and scan safety
The controller-owned implementation SHALL preserve specified catalog, search, dialog, pending-save, multi-catalog targeting, and asynchronous scan lifecycle behavior while moving decisions out of `MainFrame`.

#### Scenario: Search or Add/Update is requested
- **WHEN** a user invokes Search or Add/Update before or after the ownership refactor
- **THEN** the same active/destination catalog eligibility, dialog route, protected-catalog handling, and pending-edit semantics apply

#### Scenario: Catalog is saved or closed with pending work
- **WHEN** Save or Close requires pending-state resolution
- **THEN** the existing target-catalog Save, Discard, Cancel, failure, and refresh behavior is preserved
- **THEN** unrelated open catalogs are not modified by the delegation refactor

#### Scenario: Scan notification reaches the frame window
- **WHEN** scan progress or completion is delivered through the existing UI-thread notification boundary
- **THEN** `MainFrame` forwards the notification to the controller
- **THEN** the controller validates scan identity, resolves cancellation or accepted pending state, and directs any resulting presentation refresh
- **THEN** no stale, cancelled, or invalid result is adopted solely by frame-level logic
