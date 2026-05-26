## Why

The earlier main-frame decomposition extracted focused state and presentation collaborators, but `MainFrame` still decides catalog activation, save/discard/close behavior, media-scan initiation, and search eligibility. That leaves business rules in the UI host and makes each workflow harder to evolve or verify independently, especially with multiple open catalogs and destination-aware scans now in progress.

## What Changes

- Introduce a main-window workflow controller that owns `CatalogSession` and `ScanCoordinator` and is responsible for catalog, pending-change, media-scan, and search workflow decisions.
- Move the implementations of `ActivateCatalog`, `OnSaveCatalog`, `CloseCatalog`, `ConfirmPendingChanges`, `OnAddOrUpdateDiskImage`, and `OnSearchForItems` out of `MainFrame` into the controller, along with supporting state and decision helpers required by those flows.
- Reduce `MainFrame` command/message handling to accepting UI events, requesting controller actions, presenting requested dialogs/messages where the controller outcome requires UI interaction, and applying refresh instructions to `BrowserController` and `MainWindowChrome`.
- Express controller results as explicit UI effects/state updates so `MainFrame` does not branch on catalog editability, pending-change policy, scan-target validity, search availability, or save/close eligibility.
- Preserve existing multi-catalog targeting, staged Save/Discard semantics, scan cancellation and UI-thread completion safety, search behavior, command routes, prompts, and visible refresh behavior.

## Capabilities

### New Capabilities

- `main-workflow-controller`: Defines the controller ownership and thin-shell delegation boundary for catalog, search, and scan workflows previously decided in `MainFrame`.

### Modified Capabilities

None. Existing user-visible catalog, scan, dialog, and search workflows are preserved; this change strengthens the internal responsibility boundary.

## Impact

- `src/app/MainFrame.h` and `src/app/MainFrame.cpp` lose workflow ownership and direct ownership/use of `CatalogSession` and `ScanCoordinator`.
- A new controller class under `src/app` owns session and scan collaborators, emits results/effects for UI rendering, and is added to `WhereIsThat.vcxproj`.
- `BrowserController` and `MainWindowChrome` remain presentation/navigation collaborators invoked by the frame from controller-issued refresh outcomes.
- The change interacts with active work for multi-catalog workspace and safe scan delivery, but does not change database schema, persisted settings format, command IDs, or runtime dependencies.
