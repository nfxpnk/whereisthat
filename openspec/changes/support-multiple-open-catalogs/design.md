## Context

The implemented application already opens user-selected SQLite catalog files, stages Add/Update scans into a working `Database`, and commits them only through Save. Its boundaries are sound but singular:

- `CatalogSession` owns one `database_`, one `pendingDatabase_`, one `dirty_` flag, and one `activePath_`.
- `BrowserController` and `CatalogTreeView` receive one database pointer and label; `CatalogTreeView::Reload()` clears the tree and inserts one top-level root.
- `BrowserLocation` identifies a disk/folder within a database but cannot identify which open database supplies that location.
- `MainFrame` replaces the active session on New/Open/Open Recent and uses one-session Save/discard prompts.
- `ScanCoordinator` returns a staged database without an owning-catalog identity.
- `AddNewDiskMediaDialog` returns the selected media only; Add/Update always targets the singleton session.

The solution must preserve the native WTL/Win32 UI, SQLite schema, explicit Save semantics, UI-thread adoption of scan results, and focused collaborator boundaries established in `docs/spec/architecture.md` and prior changes.

## Goals / Non-Goals

**Goals:**
- Keep several SQLite catalog database files open concurrently with one visible TreeView root per open file.
- Route browsing, Save, Close, Add/Update, status, and prompt behavior to a stable selected/active catalog identity.
- Close one selected catalog safely, including confirmation and per-catalog pending changes.
- Allow Add New Disk/Media to select its destination among editable open catalogs.
- Retain asynchronous staged scanning without cross-catalog result adoption or connection lifetime errors.

**Non-Goals:**
- Change the on-disk SQLite catalog schema, merge catalogs, transfer media entries between databases, or persist a complete multi-open workspace across restarts.
- Run multiple scans concurrently.
- Implement currently placeholder catalog-manager, setup, rebuild, or Save As features.
- Store `HTREEITEM` values or window behavior in the storage layer.

## Decisions

### 1. Turn `CatalogSession` into the workspace owner of per-catalog state

Keep the existing session boundary, but replace its singleton database fields with stable heap-owned `OpenCatalog` objects and an active `CatalogId`. Settings and recent paths remain workspace-level state. A catalog object owns its saved database connection, staged working database, path, editable/dirty state, and user-facing label:

```cpp
using CatalogId = std::uint64_t;

struct OpenCatalog {
    CatalogId id{};
    std::wstring path;
    std::wstring label;
    wit::storage::Database database;
    std::unique_ptr<wit::storage::Database> pendingDatabase;
    bool dirty{};

    wit::storage::Database* WorkingDatabase() {
        return pendingDatabase ? pendingDatabase.get() : &database;
    }
};

class CatalogSession {
public:
    OpenCatalog* Open(const std::wstring& path, bool createNew, bool persistPath, bool& settingsSaved);
    OpenCatalog* Find(CatalogId id);
    OpenCatalog* ActiveCatalog();
    OpenCatalog* GetCatalogFromTreeItem(CatalogId id);
    void SetActive(CatalogId id);
    bool SaveCatalog(OpenCatalog& catalog);
    void DiscardPending(OpenCatalog& catalog);
    bool RemoveCatalog(CatalogId id); // only after prompts/scan guards succeed
    std::vector<OpenCatalog*> OpenCatalogs();
private:
    std::vector<std::unique_ptr<OpenCatalog>> catalogs_;
    CatalogId activeCatalogId_{};
    CatalogId nextCatalogId_{1};
};
```

`Open()` normalizes/case-fold compares file paths using Windows path semantics: selecting an already open file focuses the existing catalog instead of creating a second connection or duplicate root. A successful newly opened catalog is added without prompting about dirty state in other open catalogs. Startup restores the stored last-used catalog as one initial root; this change does not silently reopen every catalog from the prior process.

Although a root tree item must exist for every open catalog, it is view state rather than catalog state: `CatalogTreeView` maps `CatalogId` to its `HTREEITEM`. Alternative considered: store `HTREEITEM rootItem` directly in `OpenCatalog`; rejected because it makes the UI-free session own an invalidatable native control handle.

### 2. Give every tree and browser target a catalog identity

Introduce a browser target wrapper rather than polluting storage queries with database identity:

```cpp
struct BrowserTarget {
    wit::app::CatalogId catalogId{};
    BrowserLocation location;
    bool operator==(const BrowserTarget&) const = default;
};

struct CatalogTreeView::Node {
    wit::app::CatalogId catalogId{};
    BrowserLocation location;
    bool isCatalogRoot{};
    bool populated{};
};
```

`CatalogTreeView` gains `AddCatalog`, `RefreshCatalog`, `RemoveCatalog`, `CatalogIdForItem`, `IsCatalogRoot`, and `TargetForItem`; each node is expanded through the working database resolved for its `catalogId`. `BrowserController` keeps `BrowserTarget` in current location and history, tells `MainFrame` when tree selection establishes a new active catalog, and binds the ListView only to that catalog's `WorkingDatabase()`. The address line begins with that catalog's label as it does today.

Alternative considered: keep only one database bound to the browser and reconstruct the tree whenever selection crosses a root; rejected because top-level roots must remain visible and it creates easy opportunities for a child node to be queried through the wrong database pointer.

### 3. Define active catalog from TreeView selection and centralize command resolution

`GetActiveCatalog()` resolves the selected tree item's owning `CatalogId`; if the tree temporarily has no item selected after removal, it selects a deterministic remaining root (next sibling, otherwise previous/first root) and uses that catalog. Commands that read or modify catalog content obtain the resolved object immediately before acting.

`MainFrame` coordinates UI behavior through focused helpers:

```cpp
OpenCatalog* MainFrame::GetActiveCatalog();
bool MainFrame::CanCloseCatalog(const OpenCatalog& catalog) const;
bool MainFrame::ConfirmCloseCatalog(OpenCatalog& catalog);
bool MainFrame::CloseCatalog(OpenCatalog& catalog);
bool MainFrame::SaveCatalog(OpenCatalog& catalog);
void MainFrame::RefreshCatalogCommands();
```

Save, Search, Add/Update defaults, status lock/modified state, and the Save toolbar button apply to the active catalog. File `Close` is disabled with no open catalogs. Closing one root removes only that root; closing the last clears list/address/history and disables catalog-dependent commands.

Alternative considered: maintain a separate global "last used" active connection unrelated to selection; rejected because the requested UI establishes the tree as the source of catalog context.

### 4. Close uses root-only context action and two deliberate prompt gates

On a right-click, the existing hit-test first selects the clicked item. `Close Catalog` is appended/enabled only when `CatalogTreeView::IsCatalogRoot(item)` is true; child disk/folder menus omit it (or may show it disabled if consistent menu positioning is preferred). `ID_WIT_FILE_CLOSE` invokes the same close helper for `GetActiveCatalog()`, so selecting any descendant closes its owning root.

Closing a catalog follows:

```cpp
bool MainFrame::CloseCatalog(OpenCatalog& catalog) {
    if (scans_.Targets(catalog.id)) return ReportScanStillRunning();
    if (MessageBoxW(m_hWnd, L"Are you sure you want to close this catalog?",
            L"Close Catalog", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) {
        return false;
    }
    if (catalog.dirty) {
        const int choice = MessageBoxW(m_hWnd, L"Save changes?",
            L"Unsaved Catalog Changes", MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONWARNING);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES && !SaveCatalog(catalog)) return false;
        if (choice == IDNO) session_.DiscardPending(catalog);
    }
    browser_.RemoveCatalog(catalog.id);
    session_.RemoveCatalog(catalog.id); // Database destructors close handles here.
    browser_.SelectFallbackCatalog();
    RefreshCatalogCommands();
    return true;
}
```

The close confirmation is shown before the unsaved prompt and defaults to `No`. `Yes` on the unsaved prompt is supported because Save already exists; if it fails, pending state and the tree root remain. Application exit iterates modified open catalogs through Save/Discard/Cancel protection, but does not require one "Are you sure" catalog-close confirmation per root because the window-close intent is already explicit.

Alternative considered: reuse only the existing unsaved prompt as close confirmation; rejected because a clean catalog also requires an intentional close confirmation.

### 5. Make scans destination-aware while retaining one running worker

`AddNewDiskMediaResult` includes a `CatalogId destinationCatalogId`. The dialog receives value-based choices derived from open editable catalogs and preselects the active editable catalog:

```cpp
struct CatalogChoice { CatalogId id; std::wstring label; std::wstring path; };
struct AddNewDiskMediaResult {
    // existing media fields...
    CatalogId destinationCatalogId{};
};

bool Show(HWND owner, HINSTANCE instance,
    const std::vector<CatalogChoice>& choices, CatalogId preferred, AddNewDiskMediaResult& result);
```

The dialog resource adds a `Catalog:` label and combo box immediately after `Disk name:` in tab/layout order. `OK` requires both a valid media source and a selected editable destination. The active catalog is merely the default: choosing another destination deliberately stages changes there.

`ScanCoordinator::Start()` receives and stores the destination `CatalogId`; `ScanResult` returns it. A scan creates its working copy from that destination catalog's current working database, preserving existing pending edits for that catalog. While a scan runs, no second scan can start and the target catalog cannot be closed, saved, or discarded. Users may still select/browse or close an unrelated catalog. On completion, `MainFrame` calls `AcceptPending(destinationId, pending)` only if the same target session remains valid; it refreshes the target root, and refreshes the right pane/status immediately only when that target is currently displayed.

Alternative considered: forbid all multi-catalog navigation while a scan runs; rejected because it removes much of the value of multiple open catalogs and is unnecessary when target identity is carried with the result.

### 6. Refresh state narrowly and preserve storage boundaries

No SQLite schema change is needed: each `OpenCatalog` continues to own a normal `Database` connection to one file and a possible staged working copy. Storage queries remain parameterized by `BrowserLocation` once the calling controller has chosen the correct database.

`RefreshCatalogCommands()` updates menu/toolbar availability from active state: Save requires an active editable catalog (and may remain enabled while clean according to existing style), Close requires any active catalog, Add/Update requires an editable open destination and no active scan, and catalog-empty state clears browser output. The status bar renders `Loaded`/`Modified` and protection for the selected catalog, not an aggregate.

Alternative considered: merge database results into one shared tree query model; rejected because separate database connections and independent pending state are the isolation boundary.

## Risks / Trade-offs

- [Raw pointers to a catalog become invalid when one root closes] -> Heap-own catalog objects behind stable IDs, resolve IDs at each command/message boundary, and clear/rebind browser views before erasing the object.
- [A scan completion arrives after the user tries to close its destination] -> Record the target `CatalogId`, block close/save/discard for that target until completion or cancellation retirement, and discard any stale result defensively.
- [Two paths name the same database using case or relative-path differences] -> normalize absolute paths and compare with Windows case-insensitive semantics before opening a new session.
- [History refers to a closed catalog] -> remove history entries for the closed `CatalogId`, then select a surviving root or clear the right pane.
- [A dirty non-selected catalog is easy to overlook] -> keep `Modified` state per root (status when selected and optionally root label decoration later) and prompt each dirty catalog during application shutdown.
- [Dialog can target a catalog that becomes unavailable after it opens] -> modal dialog prevents same-window close while active; validate the destination ID/editability again before starting the worker.

## Migration Plan

1. Add the new capability specifications and update canonical architecture/UI/acceptance text during implementation to describe multiple open catalogs and active-by-tree-selection behavior.
2. Introduce `CatalogId`/`OpenCatalog` collection ownership in `CatalogSession`, retaining existing single-catalog behavior through the first object while adapting Save/discard/settings APIs.
3. Add catalog-aware tree/browser targets and incremental root add/remove/refresh; then switch New/Open/Open Recent to add-or-focus behavior.
4. Implement root-only context Close, File Close, safe confirmations, per-catalog Save/discard, command state, empty workspace, and shutdown resolution.
5. Add destination selection to Add New Disk/Media and carry the destination identity through scan start/completion/adoption.
6. Build x64 Release and smoke test independent open, browse, edit, save, close, scan, read-only, duplicate-path, and shutdown scenarios.

No persisted data migration is required. Rollback can revert to single-open application behavior; SQLite files saved by this change remain ordinary valid catalog files.

## Open Questions

- Should a later usability change decorate dirty non-active tree roots (for example with `*`) so their pending state is visible without selecting them? This does not block correct per-catalog close and shutdown prompting.
