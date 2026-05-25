## ADDED Requirements

### Requirement: WTL-hosted main window infrastructure
The system SHALL use the already vendored WTL 10.01 and installed ATL native infrastructure for primary main-frame window hosting and message routing, while continuing to render a native Win32/Common Controls desktop interface with its existing resources and observable behavior.

#### Scenario: Application main window is created
- **WHEN** the application starts its primary main window
- **THEN** top-level frame creation and message routing are hosted through WTL/ATL native window infrastructure
- **THEN** the user receives the existing native main-window controls, menu, toolbar, status area, and layout behavior

#### Scenario: Frame routes existing interactions
- **WHEN** the frame receives an existing menu, accelerator, toolbar, control notification, drawing, splitter, or scan-completion message
- **THEN** the corresponding existing workflow is dispatched through the WTL-hosted routing path
- **THEN** resource command identifiers and user-visible workflow semantics remain unchanged

### Requirement: WTL message-loop and accelerator lifetime
The system SHALL integrate the primary UI thread's message loop and existing accelerator processing with `WTL::CAppModule` and WTL message-loop/filter infrastructure, without adding a deployed WTL runtime.

#### Scenario: Primary UI loop runs
- **WHEN** the native application has initialized its WTL application module and created the primary frame
- **THEN** its UI-thread loop is registered and run through WTL message-loop infrastructure
- **THEN** existing accelerators continue to route to their established commands

#### Scenario: Application shuts down
- **WHEN** the main frame closes and the UI thread exits
- **THEN** the WTL loop and module lifetime are cleaned up consistently
- **THEN** catalog and settings shutdown semantics remain unchanged

### Requirement: Decomposed ownership survives WTL migration
The system SHALL preserve the established main-window responsibility split while adopting WTL: `MainFrame` remains a composition and coordination boundary, `MainWindowChrome` remains responsible for child presentation/layout, `BrowserController` for browsing/navigation, `CatalogSession` for active/pending catalog state, and `ScanCoordinator` for asynchronous scanning.

#### Scenario: Main frame is migrated to WTL
- **WHEN** WTL frame hosting and message maps replace raw top-level window plumbing
- **THEN** browser history, catalog session state, worker ownership, and child chrome implementation are not consolidated back into `MainFrame`

#### Scenario: Native background work completes
- **WHEN** `ScanCoordinator` posts scan progress or completion to the WTL-hosted main window
- **THEN** the UI-thread coordinator safely applies the same pending-session, browser-refresh, and status-refresh behavior as before migration
