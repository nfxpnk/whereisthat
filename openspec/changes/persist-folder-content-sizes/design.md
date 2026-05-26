## Context

The right-pane contents list is a native `LVS_OWNERDATA` view that obtains count and page data synchronously from `Database` as Windows requests display text. Folder records currently do not have a size value. A recently reverted attempt supplied folder sizes with a correlated recursive `SUM(files.size)` subquery inside `GetBrowserItemsPage()`: on the configured `Q:\tttt.db` catalog, displaying `Q:\` required recursive work across descendant contents for each immediate folder while the UI thread waited for the first page.

The normalized schema already makes `folders` and `files` persisted scan results rather than live filesystem views. `FileScanner` runs on the background scan path and writes content to a staged SQLite database inside the scan transaction. The user has explicitly selected a breaking new-format approach: catalog files lacking the new folder-size representation need not be opened, converted, or backfilled.

Future item sorting must remain compatible with owner-data paging. A page cache cannot globally sort rows in memory, particularly when folder sizes and file sizes originate from persisted catalog records.

## Goals / Non-Goals

**Goals:**

- Store each folder's complete contained-file byte total, including files in descendant folders, as normalized catalog data.
- Populate folder totals during the background scan/staging workflow so browsing never computes recursive directory totals.
- Display folder sizes and status-bar size reporting from persisted values while preserving offline browsing.
- Ensure contents paging resolves the selected stored folder through indexed relationships and remains suitable for future SQL `ORDER BY size` behavior.
- Keep the replacement format deliberate and simple by rejecting catalog files that do not contain the newly required folder-size field.

**Non-Goals:**

- Preserve, migrate, repair, or backfill catalogs produced before this field exists.
- Implement interactive column-click, toolbar, menu, ascending/descending, or persisted sort behavior in this change.
- Store allocated size, live filesystem size, or mutable cache values independent of a successful scan.
- Introduce asynchronous browse-page loading or a general database query/performance framework.

## Decisions

- Add `content_size INTEGER NOT NULL DEFAULT 0` to `folders`. `content_size` means the sum of `files.size` for files owned directly by the folder or by any stored descendant folder in the same scan result. Empty folders and retained but untraversed reparse-point directories store `0`. Alternative considered: calculate this value on each page read; rejected because it causes navigation-time subtree traversal and cannot efficiently support paged sorting.

- Treat the new field as required replacement-format storage. `InitializeSchema()` creates it, `HasCatalogSchema()` validates it, maintained schema documentation describes it, and an existing catalog missing it is rejected without alteration. Alternative considered: `ALTER TABLE` plus backfill or an optional/null size; rejected because the chosen option intentionally does not preserve existing catalog files and optional values would complicate display and later sorting semantics.

- Compute each folder total as part of scanning, before the staged scan transaction is committed. Refactor enumeration to complete a folder after its traversable descendants have been completed, carrying only in-progress aggregate values needed by traversal frames; after completion storage updates that folder's persisted `content_size`. Files continue to store their ordinary byte sizes. Alternative considered: accumulate all folder totals in the browse layer or retain an application-wide cache; rejected because it mixes catalog truth with transient UI memory. A transient scan-stack subtotal is acceptable because it is used only to create persisted scan output and is discarded before browsing.

- Make scan success atomic for aggregates. New content rows, their folder totals, disk totals, and latest scan statistics become available together only when staged scanning succeeds and is later saved under the existing session rules. A cancelled or failed scan does not expose partial or stale aggregate values. Alternative considered: asynchronously update folder totals after scan acceptance; rejected because the catalog could temporarily display incorrect values or be sorted by incomplete data.

- Read folder sizes directly from `folders.content_size` in the mixed folder/file projection and render the `Size` column for either type through the existing `FileEntry::size` display projection. Status handling shall consume the same projected stored values for focused and selected rows. Alternative considered: introduce an in-memory folder-size map joined in the view adapter; rejected because it duplicates persisted data and is incompatible with database-backed paging.

- Scope immediate-content reads through the selected folder identity. Storage first resolves the current location to a folder row using the existing unique `(disk_id, path COLLATE NOCASE)` access path, then uses `folders(disk_id, parent_folder_id)` for immediate directories and `files(folder_id)` for immediate files in count/page reads. Add narrowly justified composite indexes for future ordering only when an implemented read path benefits from them; this change does not guess all future sort indexes. Alternative considered: retain disk-wide file filtering and rely on SQLite to find the folder join cheaply; rejected because the existing plan can scan file rows for a whole disk before path filtering.

- Reserve sorting ownership for a future database-backed browser change. Once sorting is implemented, storage queries can order the union's numeric `size` expression, using `folders.content_size` and `files.size`, while `FileListView` continues caching only pages returned in authoritative order. This change supplies stable data but does not cause currently inert sort controls to reorder rows. Alternative considered: pre-sort cached view rows after retrieval; rejected because it cannot produce correct global ordering for virtual lists.

## Risks / Trade-offs

- [The on-disk format rejects current normalized development catalogs] -> Document the break explicitly and require new catalogs or a rescan into a newly created catalog; do not silently alter user-selected files.
- [Computing totals increases scan processing] -> Perform simple integer accumulation while the background scanner is already enumerating each file; trade scan-time work for fast repeated offline reads.
- [Changing scanner traversal can affect cancellation and transaction behavior] -> Keep all folder-size writes inside the existing staged transaction and extend cancellation/smoke tests for nested folders and unsuccessful scans.
- [The persisted field can become inconsistent if future editing mutates files independently of scanning] -> Treat any future file insert/delete/edit workflow as responsible for maintaining or replacing ancestor totals; current implemented content replacement is scan-transaction based.
- [Index changes may be premature for not-yet-implemented sorting] -> Correct evidenced immediate-location access now and defer per-sort composite indexes until actual ordering queries are specified and profiled.

## Migration Plan

1. Update maintained specifications and database documentation to establish required persisted folder content totals and intentional rejection of catalogs without them.
2. Extend the `folders` schema/model/storage operations with required `content_size` and structural validation.
3. Refactor the scanner's staged traversal to persist completed recursive folder totals transactionally.
4. Change right-pane page/status projection to consume the stored total and revise immediate-location reads to use indexed folder ownership.
5. Add focused storage smoke cases for nested totals, empty folders, failed/cancelled staging, unsupported earlier normalized catalogs, and paging without navigation-time aggregation; build and manually verify responsive browsing.

Rollback is source-only and is expected to make catalogs created with `content_size` unusable to the reverted executable, as this is an intentionally breaking replacement-format change.

## Open Questions

None block implementation. Interactive sorting should be proposed separately once its exact folder-first/order/tie-breaking and persistence behavior are chosen.
