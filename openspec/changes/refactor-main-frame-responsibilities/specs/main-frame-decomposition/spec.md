## ADDED Requirements

### Requirement: Main frame remains an orchestration boundary
The system SHALL keep `MainFrame` as the top-level main-window lifetime, message/command coordination, and component-composition boundary, and SHALL implement distinct main-window chrome/layout, catalog browser/navigation, catalog-session lifecycle, and asynchronous scan coordination responsibilities in focused collaborating classes rather than accumulating them in `MainFrame`.

#### Scenario: Refactored main window is implemented
- **WHEN** the main-window refactor is complete
- **THEN** `MainFrame` composes focused collaborators for chrome/layout, browser/navigation, catalog-session lifecycle, and asynchronous scan coordination
- **THEN** the state and implementation for each extracted responsibility are owned by its collaborator rather than remaining implemented in `MainFrame`

#### Scenario: Future main-window behavior is introduced
- **WHEN** a subsequent change introduces behavior belonging to chrome/layout, browser/navigation, catalog-session lifecycle, or asynchronous scan coordination
- **THEN** that behavior is added to its responsible collaborator or to a newly justified focused component
- **THEN** `MainFrame` remains limited to top-level lifetime and cross-component coordination

### Requirement: Extracted components preserve current behavior
The system SHALL preserve the existing user-visible main-window workflows and native asynchronous safety semantics while decomposing `MainFrame`, including catalog lifecycle, staged Save/Discard, browser navigation, menu and toolbar dispatch, status presentation, settings/recent catalog behavior, and scan completion delivery on the UI thread.

#### Scenario: Existing main-window workflows are exercised after decomposition
- **WHEN** the user creates or opens a catalog, browses persisted locations, invokes existing command routes, stages and saves or discards scan changes, changes supported settings, or closes with pending changes
- **THEN** the workflow behavior remains consistent with the behavior specified before the decomposition

#### Scenario: Background scan reports results after decomposition
- **WHEN** asynchronous disk/media scanning reports progress or completion
- **THEN** background work does not block the frame message pump
- **THEN** UI and active-session updates are accepted through the UI-thread coordination boundary

### Requirement: Refactor establishes the boundary for later WTL migration
The system SHALL treat migration of remaining raw main-window/frame and control presentation code to the already available WTL 10.01 infrastructure as a follow-on architectural change, using the decomposed ownership boundaries rather than combining that conversion with this behavior-preserving extraction.

#### Scenario: Main-frame decomposition is delivered
- **WHEN** this refactor is implemented and verified
- **THEN** it does not require converting the frame or child-control implementation to WTL wrappers as part of the same delivery
- **THEN** the extracted window-facing components provide the responsibility boundaries for a subsequent WTL migration proposal
