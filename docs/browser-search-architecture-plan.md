# Browser and Search Architecture Plan

This plan captures the reviewed architecture, performance risks, and practical
refactoring path for the Where Is That? catalog browser and search UI.

The goal is not to replace WTL/ATL, SQLite, or the current module layout. The
goal is to make the existing native app architecture safer, more consistent,
more testable, and better suited to large offline catalogs.

## Current Architecture Summary

The app already follows the intended broad shape:

- `src/app/wit_gui` owns WTL/ATL windows, dialogs, panes, and UI message routing.
- `BrowserController` coordinates tree selection, file-list navigation, history,
  address text, and selected catalog state.
- `CatalogTreeView` adapts a Win32 TreeView to catalog roots, disk groups, disks,
  and folder nodes.
- `FileListView` adapts a Win32 owner-data ListView to browser root rows and
  folder content rows.
- `SearchDialog` adapts a dialog-hosted owner-data ListView to search results.
- `wit_database` owns SQLite catalog browsing and write queries.
- `wit_search` owns search query preparation and search paging.
- `wit_types` provides shared `FileEntry`, `BrowserLocation`, `Disk`, and
  `FileSort` types.

The main browser path is:

```text
MainFrame
  -> BrowserController
  -> CatalogTreeView / FileListView
  -> Database::BrowserRepository()
  -> SqliteBrowserRepository
```

The search path is:

```text
SearchDialog
  -> ISearchRepository
  -> SqliteSearchExecutor
```

The file list and search result list already use `LVS_OWNERDATA` and database
pages. This is the right direction for a large catalog application.

## Verified Issues

### 1. Tree Root and Disk Group Loading Is Too Eager

Area:

- `src/app/wit_gui/src/TreeViewPane.cpp`

Current behavior:

- Folder nodes are loaded lazily when expanded.
- Catalog roots and disk groups are populated eagerly by `PopulateRoot`.
- `PopulateRoot` recursively queries disk groups and disks using
  `GetBrowserRootItemCount` and `GetBrowserRootItemsPage(..., 0, count)`.

Why it matters:

- Large catalogs with many groups or disks can do unnecessary work during open,
  refresh, or save completion.
- The TreeView root refresh can block the UI thread.
- It treats root/group nodes differently from folder nodes even though they are
  also hierarchical browse containers.

Impact:

- Performance
- UX
- Maintainability

Risk level:

- Medium

Recommended fix:

- Make disk group children lazy like folder children.
- Root refresh should only update the root node, its child indicator, and the
  currently visible/expanded subtree.
- Expansion should load the requested node's children on demand.

Fix size:

- Medium

Timing:

- Do soon, after tests for tree expansion behavior.

### 2. Main File List Has Synchronous Page Loads in UI Paths

Area:

- `src/app/wit_gui/src/FileListPane.cpp`

Current behavior:

- `SetLocation`, `ResetCachedItems`, sorting, and cache hints use async workers.
- `EntryAt` calls `CacheFilePage`, which can synchronously query
  `GetBrowserItemsPage`.
- `BrowserItemAt` can synchronously query `GetBrowserRootItemsPage`.
- `SelectEntry` scans every row and calls `EntryAt`, which can load many pages.

Why it matters:

- Owner-data display is mostly async, but selection, locate, and fallback access
  can still block the UI thread.
- `SelectEntry` can become O(total rows) and database-heavy for large folders.
- These paths are easy to call from UI actions that users expect to be instant.

Impact:

- Performance
- UX
- Correctness under large catalogs

Risk level:

- High

Recommended fix:

- Split cached access from loading access.
- Keep `CachedEntryAt` and `CachedBrowserItemAt` as non-blocking accessors.
- Replace synchronous `EntryAt` and `BrowserItemAt` use in UI code with explicit
  async ensure/load behavior.
- Add repository methods for direct lookup by stable identity where needed,
  especially locate/select by item id.
- Avoid scanning all rows for selection restoration or locate operations.

Fix size:

- Medium to large

Timing:

- Do soon, after adding tests that prove locate/select does not page through
  very large result sets.

### 3. Browser Async Reads Capture Raw Repository Pointers

Area:

- `src/app/wit_gui/src/FileListPane.cpp`
- `src/modules/wit_database/include/wit_database/IBrowserRepository.h`

Current behavior:

- Browser worker lambdas capture `IBrowserRepository*`.
- `FileListView` owns a mutex that serializes its own repository calls.
- The mutex does not itself own or prove the lifetime of the database or
  repository.

Why it matters:

- This is a lifetime-safety risk if a catalog can close, rebind, or be destroyed
  while a background read is still running.
- The current code may be protected by higher-level close/drain behavior, but
  the safety is not evident at the `FileListView` boundary.

Impact:

- Correctness
- Maintainability

Risk level:

- Medium to high

Recommended fix:

- Introduce an explicit browser read handle with clear lifetime semantics.
- Options, from smallest to larger:
  - Add a catalog-session-owned lifetime token checked by workers before and
    after reads.
  - Make `FileListView::SetLocation` accept a small shared read context instead
    of a raw repository pointer.
  - Open a dedicated read-only SQLite connection for browser paging, similar in
    spirit to search's dedicated read connection.

Fix size:

- Medium

Timing:

- Do before deeper async refactoring.

### 4. Browser and Search Sorting Policy Is Not Centralized

Area:

- `src/modules/wit_database/src/SqliteBrowserRepository.cpp`
- `src/modules/wit_database/src/SqliteFileListHelpers.cpp`
- `src/modules/wit_search/src/SqliteSearchExecutor.cpp`

Current behavior:

- Search combines folders and files into one temp result set and uses
  `FileEntryOrderBy`.
- Browser content uses a separate local `BrowserOrderByFor`.
- Browser content fetches folders first and files second. That may be an
  intentional Explorer-like policy.
- Browser root rows already use `WIN_NATURAL_NOCASE`, but browser folder/file
  content name sorting does not use the shared natural collation path.

Why it matters:

- The policy is spread across modules.
- Browser and search can sort the same conceptual file/folder columns
  differently.
- Future sort fixes must be made in multiple places.

Impact:

- UX
- Maintainability
- Testability

Risk level:

- Medium

Recommended fix:

- Document the intended policy:
  - Browser content: folders first, files second, then selected column order.
  - Search results: combined folder/file result set, sorted globally.
- Centralize shared column expressions and tie-breakers where possible.
- Add a dedicated browser content order helper rather than leaving ad hoc SQL in
  `SqliteBrowserRepository`.
- Ensure browser content name/type/path text sorts use the intended collation.

Fix size:

- Medium

Timing:

- Do after repository sort tests are expanded.

### 5. Search Results Are Paged After Full Match Materialization

Area:

- `src/modules/wit_search/src/SqliteSearchExecutor.cpp`

Current behavior:

- Search creates `wit_search_page_cache`.
- Each new search/sort materializes all matching folders/files into that temp
  cache.
- Later pages read by position from the temp cache.

Why it matters:

- This is not naive paging; it gives stable search snapshots and is tested.
- Broad searches still pay the full materialization cost before the first page
  is ready.
- Large temp result sets can affect search latency and memory/temp storage.

Impact:

- Performance
- UX

Risk level:

- Medium for normal searches, higher for broad searches on huge catalogs

Recommended fix:

- Keep the current stable snapshot design for now.
- Improve instrumentation, cancellation visibility, and tests around broad
  searches.
- Defer bigger changes such as FTS, trigram indexes, or incremental result
  streaming until measured evidence justifies them.

Fix size:

- Small to medium now, large if changing search indexing later

Timing:

- Instrument and test soon.
- Defer search engine redesign.

### 6. File List and Search List Duplicate Owner-Data Infrastructure

Area:

- `src/app/wit_gui/include/wit_gui/FileListPane.h`
- `src/app/wit_gui/include/wit_gui/SearchPane.h`
- `src/app/wit_gui/src/FileListPane.cpp`
- `src/app/wit_gui/src/SearchPane.cpp`

Current behavior:

- Both lists implement page cache structs, max cached page count, worker
  lifetimes, reaper threads, mailboxes, page-ready messages, sort indicators,
  text formatting, selection summaries, and column width persistence.

Why it matters:

- Bugs fixed in one list can remain in the other.
- Testing the paging and cache behavior requires UI-heavy tests.
- The code makes it harder to reason about owner-data invariants.

Impact:

- Maintainability
- Testability

Risk level:

- Medium

Recommended fix:

- Extract a small reusable helper for owner-data page state:
  - page range normalization
  - LRU page cache
  - pending page start
  - request id invalidation
  - non-blocking cached row access
- Keep WTL/Win32 control code in the panes.
- Do not introduce a new UI framework.

Fix size:

- Medium

Timing:

- Do after behavior tests exist for both panes.

### 7. Browser Repository Error Reporting Is Weak

Area:

- `src/modules/wit_database/src/SqliteBrowserRepository.cpp`

Current behavior:

- Browser repository methods generally return `0` or an empty vector on
  failure.
- Search has `LastErrorMessage`, but browser reads do not expose equivalent
  detail.

Why it matters:

- SQL failure, missing schema, interrupted operation, and real empty folders can
  all look like an empty UI.
- Blank-screen regressions are harder to diagnose.

Impact:

- Correctness
- UX
- Testability

Risk level:

- Medium

Recommended fix:

- Add a browser read result type or `LastErrorMessage`-style reporting.
- Keep empty folders represented as successful empty results.
- Log detailed SQLite errors at the repository boundary.
- Surface a lightweight empty/error state in the UI.

Fix size:

- Medium

Timing:

- Do before larger repository rewrites.

### 8. SQLite Validation Helpers Need RAII Statement Lifetime

Area:

- `src/modules/wit_database/src/Database.cpp`
- `src/modules/wit_database/src/CatalogSchema.cpp`
- `issues/WIT-017.txt`

Current behavior:

- `IntegrityCheckOk`, `PragmaReturns`, and `TableHasColumn` use raw
  `sqlite3_stmt*` and manual `sqlite3_finalize`.

Why it matters:

- Exceptions from allocation/conversion/comparison between prepare and finalize
  can skip `sqlite3_finalize`.
- Leaked statements can keep SQLite resources alive and complicate close
  behavior.

Impact:

- Resource safety
- Maintainability

Risk level:

- Low

Recommended fix:

- Use existing `SQLiteStatement` RAII or a small local statement guard.
- Preserve current return values and validation behavior.

Fix size:

- Small

Timing:

- First patch.

## Target Architecture

The target architecture should remain conservative and native:

```text
WTL/ATL UI
  -> Browser/Search pane adapters
  -> Shared owner-data page cache helper
  -> Browser/Search data providers
  -> SQLite repositories
  -> Shared catalog types
```

### UI Layer

Responsibilities:

- WTL/ATL message maps.
- Control creation and layout.
- TreeView/ListView notifications.
- Display text, images, status bar text.
- User command routing.

Non-responsibilities:

- Durable SQL.
- SQLite schema knowledge.
- Long-running query execution on the UI thread.
- Full result materialization.

### Browser Controller

Responsibilities:

- Current `BrowserTarget`.
- Navigation history.
- Address rendering.
- Tree/list synchronization.
- Routing selection and activation commands.

Future improvements:

- Keep it as orchestration, not SQL owner.
- Avoid using list row scans for locating items.
- Route locate/select through repository-backed identity lookups.

### Tree Provider

Responsibilities:

- Provide children for catalog root, disk group, disk root, and folder nodes.
- Return child count or child indicator without loading full subtrees.
- Maintain enough identity to restore expanded nodes after refresh.

Future improvements:

- Treat disk groups and folders consistently as lazy containers.
- Keep TreeView item data small and stable.

### List Row Model

Add a small row model that both browser and search can use:

```cpp
struct FileListRow {
    std::int64_t id{};
    std::int64_t sourceId{};
    bool isDirectory{};
    bool isArchive{};
    std::wstring name;
    std::wstring typeLabel;
    std::wstring parentPath;
    std::wstring fullPath;
    std::uint64_t size{};
    std::int64_t modifiedAt{};
    std::uint32_t attributes{};
};
```

This does not need to replace `FileEntry` immediately. It can be introduced as
a presentation boundary type after query tests are in place.

### Shared Owner-Data Page Helper

Introduce a helper that is independent of WTL controls:

Responsibilities:

- Normalize row index to page start.
- Store cached pages with LRU eviction.
- Track request generation.
- Track pending page requests.
- Expose non-blocking `CachedRowAt`.
- Never do UI work.
- Never own HWND.

The panes remain responsible for:

- Posting/handling window messages.
- `ListView_SetItemCountEx`.
- `ListView_RedrawItems`.
- Column definitions and header state.

### Repository Layer

Responsibilities:

- SQL query construction.
- SQLite statement ownership.
- Mapping rows to shared data types.
- Error reporting.
- Sorting policy in SQL.
- Paged reads.

Future improvements:

- Browser repository should expose explicit success/error results.
- Browser read lifetime should be explicit.
- Search can continue using a prepared page cache until measured data proves a
  different strategy is needed.

## Step-by-Step Refactoring Roadmap

### Phase 0: Baseline and Guardrails

Goal:

- Confirm the current behavior and establish tests before changing structure.

Steps:

1. Record current known behavior in this plan.
2. Keep the existing WTL/ATL, SQLite, MSBuild, and GoogleTest stack.
3. Do not introduce new dependencies.
4. Do not rewrite the panes wholesale.
5. Keep unrelated untracked import/test artifacts untouched.

Acceptance criteria:

- Plan is committed or available in `docs`.
- No code behavior changes in this phase.

### Phase 1: Patch WIT-017 Statement RAII - Complete

Goal:

- Fix the lowest-risk resource safety issue first.

Steps:

1. Replace manual `sqlite3_stmt*` handling in `IntegrityCheckOk`.
2. Replace manual `sqlite3_stmt*` handling in `PragmaReturns`.
3. Replace manual `sqlite3_stmt*` handling in `TableHasColumn`.
4. Prefer the existing `SQLiteStatement` wrapper if it supports these cases.
5. Preserve current return behavior.
6. Add or update focused tests only if practical without overfitting to
   implementation details.
7. Build `UnitTests`.

Status:

- Complete. `IntegrityCheckOk`, `PragmaReturns`, and `TableHasColumn` now use `SQLiteStatement` RAII.

Acceptance criteria:

- No raw `sqlite3_finalize` remains in those three helpers.
- Existing schema validation behavior is unchanged.
- `msbuild UnitTests.vcxproj /p:Configuration=Debug /p:Platform=x64` succeeds.

### Phase 2: Add Repository Behavior Tests - Complete

Goal:

- Lock down browser/search data behavior before moving logic.

Steps:

1. Expand `BrowserRepositoryTests`.
2. Cover empty folders.
3. Cover missing folder path behavior.
4. Cover duplicate names.
5. Cover duplicate paths where schema allows it or verify uniqueness behavior.
6. Cover folders and files with natural numeric names.
7. Cover all browser content sort columns:
   - name
   - type
   - size
   - path
   - modified
8. Cover root/disk group sorting separately from content sorting.
9. Add tests for folder-first browser content policy.
10. Add tests showing search global sort policy separately.

Status:

- Complete. `BrowserRepositoryTests` now covers empty and missing folders, duplicate names, duplicate path schema assumptions, natural names, all browser content sort columns, root/group paging, and the browser folder-first policy. Existing `SearchExecutor` coverage documents the separate global search sort policy.

Acceptance criteria:

- Tests describe intentional differences between browser content sorting and
  search result sorting.
- Blank-page regression cases remain covered.
- Tests pass before refactoring sort helpers.

### Phase 3: Centralize Formatting - Complete

Goal:

- Remove duplicated row text/status formatting without changing behavior.

Steps:

1. Create a small helper in an appropriate module, likely `wit_gui` first.
2. Move shared type label logic:
   - archive
   - folder
   - extension/file
3. Move shared size formatting wrappers used by browser and search panes.
4. Move shared date formatting calls only where it reduces duplication.
5. Move shared `FileEntryStatusText` if needed.
6. Update `FileListPane` and `SearchPane` to call the helper.
7. Keep SQL and ListView code unchanged.

Status:

- Complete. Browser and search result type labels now share `FileEntryTypeText`; existing shared size and status helpers remain unchanged.

Acceptance criteria:

- Browser and search display the same type/size/date text as before.
- Tests that inspect status or displayed text still pass.
- No new dependency crosses from database/search modules into GUI.

### Phase 4: Centralize Sorting Policy - Complete

Goal:

- Make sorting deliberate and testable.

Steps:

1. Document browser content policy:
   - folders first
   - files second
   - selected sort inside each group
2. Document search policy:
   - combined folder/file result set
   - selected sort globally
3. Extract browser content SQL order helper from
   `SqliteBrowserRepository.cpp`.
4. Keep search using `FileEntryOrderBy` unless tests show a needed change.
5. Ensure browser content text sorts use the intended collation.
6. Run all repository and search tests.

Status:

- Complete. Browser content SQL ordering now lives in `SqliteFileListHelpers` as `BrowserContentOrderExpression` / `BrowserContentOrderBy`, while search keeps `FileEntryOrderBy`. Browser content remains folders-first and files-second, with natural no-case collation applied to browser text sort keys and name tie-breakers.

Acceptance criteria:

- Sort SQL is no longer ad hoc inside the main repository query method.
- Tests clearly verify both browser and search sort policies.
- Browser sort by path/type/name cannot return a blank page.

### Phase 5: Add Browser Error Reporting - Complete

Goal:

- Distinguish true empty results from failed reads.

Steps:

1. Decide on API shape:
   - result object, or
   - `LastErrorMessage`, matching search style.
2. Add SQLite error capture in `SqliteBrowserRepository`.
3. Preserve current methods temporarily if needed for compatibility.
4. Add tests that force browser query failure and verify an error is recorded.
5. Update `FileListView` to display or log a lightweight error state.
6. Keep empty folder behavior as successful empty result.

Status:

- Complete. Browser repository reads now expose `LastErrorMessage()`, capture SQLite prepare/step failures, keep empty folders and missing paths as successful empty results, and log async file-list read errors through the UI layer without changing existing paging APIs.

Acceptance criteria:

- Empty folders and failed queries are distinguishable.
- UI no longer silently treats all failures as empty lists.
- Existing callers still compile during transition.

### Phase 6: Make Browser Async Lifetime Explicit - Complete

Goal:

- Make background browser reads safe by construction.

Steps:

1. Audit catalog close, refresh, and database rebind flows.
2. Identify where database/repository lifetime is currently guaranteed.
3. Add a small read context type, for example:

   ```cpp
   struct BrowserReadContext {
       std::shared_ptr<void> lifetimeToken;
       wit::storage::IBrowserRepository* repository{};
   };
   ```

4. Replace raw repository pointer passing to `FileListView` with that context.
5. Ensure workers check request id and lifetime token before publishing results.
6. Consider a dedicated read-only SQLite connection for browser paging if the
   lifetime token still leaves threading unclear.
7. Add tests around rebinding/closing during a blocked read.

Status:

- Complete. Browser async reads now use `BrowserReadContext` with an explicit `BrowserReadToken` minted by `Database`, invalidated on close/rebind, and checked by file-list workers before repository reads and before publishing results. Tests cover an invalidated blocked read being discarded.

Acceptance criteria:

- `FileListView` no longer relies only on a raw repository pointer for async
  work.
- Rebinding or closing during a read cancels or safely discards results.
- Existing async performance tests still pass.

### Phase 7: Remove Synchronous UI-Thread Page Reads - Complete

Goal:

- Make owner-data lists non-blocking in normal UI access paths.

Status:

- Complete. `EntryAt` and `BrowserItemAt` now return cached rows only and schedule async page loads on misses, removing the remaining synchronous UI-thread page reads. `SelectEntry` searches cached pages only instead of scanning every row, and large-list tests cover bounded page access.

Steps:

1. Rename current synchronous helpers to make blocking behavior explicit, or
   remove them.
2. Keep `CachedEntryAt` and `CachedBrowserItemAt` as non-blocking helpers.
3. Replace display, status, context menu, activation, and selection paths with:
   - cached access when available
   - async schedule when missing
   - graceful temporary empty text/image while loading
4. Replace `SelectEntry` row scan with a repository lookup:
   - locate row by item id under current sort, or
   - navigate to parent and select after loading the relevant page.
5. Limit selection restoration to cached or visible rows unless a targeted row
   lookup is available.
6. Add tests with very large fake lists verifying no synchronous page fan-out.

Acceptance criteria:

- UI display callbacks do not synchronously query SQLite.
- Locate/select does not scan every row in a large folder.
- Large-list tests prove page count stays bounded.

### Phase 8: Lazy Load Disk Groups in the Tree - Complete

Goal:

- Make root/group tree behavior scale like folder behavior.

Status:

- Complete. Catalog roots now keep only a child indicator until expanded, root and disk-group expansion loads immediate children on demand, refresh invalidates displayed children instead of recursively rebuilding descendants, and tree smoke tests cover lazy root/group expansion plus nested group selection.

Steps:

1. Add a tree child provider method for root/group children.
2. Insert catalog root nodes with child indicators only.
3. On expand, load immediate disk groups and disks.
4. Mark populated nodes after successful load.
5. On refresh, invalidate affected loaded nodes rather than rebuilding every
   descendant eagerly.
6. Preserve selected node where possible.
7. Add tests or smoke checks for:
   - root expansion
   - group expansion
   - nested groups
   - moving disk/group nodes
   - refresh after scan/save

Acceptance criteria:

- Opening a catalog does not recursively query all disk groups.
- Expanding a node loads only that node's immediate children.
- Existing move/refresh behavior remains correct.

### Phase 9: Extract Shared Owner-Data Page Cache - Complete

Goal:

- Reduce duplicated async paging/cache code between browser and search panes.

Status:

- Complete. Added `OwnerDataPageCache<T>` for page normalization, cache hits, LRU page storage, pending page replacement, and request-id freshness. `FileListView` and `SearchDialog` now use the shared helper while keeping their repository-specific async workers and message handlers separate.

Steps:

1. Extract non-UI page cache logic from `FileListView`.
2. Add unit tests for:
   - page normalization
   - cache hits
   - LRU eviction
   - pending page replacement
   - stale request id rejection
3. Adapt `FileListView` to use the helper.
4. Adapt `SearchDialog` to use the helper.
5. Keep each pane's WTL message handling separate.
6. Keep each pane's repository calls type-specific.

Acceptance criteria:

- Duplicate page cache code is reduced.
- Behavior remains unchanged.
- Existing UI performance tests pass.

### Phase 10: Improve Search Performance Observability

Goal:

- Keep the stable snapshot design, but make broad searches measurable and
  controllable.

Steps:

1. Add timing/counter instrumentation around:
   - cache build start/end
   - rows materialized
   - first page ready
   - cancellation
2. Add tests for broad search cancellation.
3. Add disabled/manual benchmark coverage for large search catalogs if needed.
4. Evaluate whether indexes support common search patterns.
5. Defer FTS or alternate indexing until measurements show the need.

Acceptance criteria:

- Broad search cost is visible.
- Cancellation remains reliable.
- No search engine redesign is done without data.

## Test Plan

Use the existing GoogleTest/MSBuild setup documented in `tests/README.md`.

Build command:

```powershell
msbuild UnitTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Run command:

```powershell
.\x64\Debug\UnitTests.exe
```

### Repository Tests

Add or expand tests for:

- Browser root count and paging.
- Disk group count and paging.
- Folder content count and paging.
- Empty folders.
- Missing folder path.
- Duplicate names.
- Natural numeric names such as `item2` and `item10`.
- Sort by name.
- Sort by type.
- Sort by size.
- Sort by path.
- Sort by modified time.
- Folder-first browser policy.
- Combined search policy.
- SQLite failure reporting.

### UI Adapter Tests

Add or expand tests for:

- Main list does not load pages synchronously on cache hints.
- Main list does not load all rows during locate/select.
- Search list cancellation on repository rebind.
- Stale worker messages do not clear current results.
- Empty list state.
- Error list state.
- Column width persistence remains independent between browser and search.

### Tree Tests

Add or expand tests for:

- Root node shows child indicator without loading all children.
- Expanding root loads immediate children.
- Expanding disk group loads immediate children.
- Expanding folder loads child folders.
- Refresh preserves or safely resets selection.
- Move disk to group updates only needed nodes.
- Move group to group preserves displayed subtree where applicable.

### Performance Tests

Keep or add manual/disabled tests for:

- Opening a 500k-item folder.
- Deep scrolling in the main list.
- Sorting a large folder.
- Broad search result preparation.
- Search cancellation.
- Tree refresh with many disk groups.

## First Safe Patch

Implement WIT-017:

1. Update `IntegrityCheckOk`.
2. Update `PragmaReturns`.
3. Update `TableHasColumn`.
4. Use RAII statement lifetime.
5. Preserve behavior.
6. Build and run focused tests.

This patch is intentionally small. It improves resource safety without forcing
any of the larger architecture choices.

