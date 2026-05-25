## Context

The implemented catalog browser has a `TreeView` on the left and one owner-data `ListView` on the right. A root `BrowserLocation` is currently served through `Database::GetBrowserItemsPage()` by projecting `disks` rows into `FileEntry` folder rows, so the right pane retains the file columns (`Name`, `Type`, `Size`, `Path`, and `Modified`) even though the user is viewing catalog-level disks.

The normalized catalog schema and `wit::core::Disk` already contain the requested disk-level data: name, disk type, total/free capacity, update timestamp, number, description, category, and location. The feature must stay native Win32, use stored metadata offline, and keep the owner-data/paged presentation appropriate for large catalogs.

## Goals / Non-Goals

**Goals:**
- Make catalog-root selection present a clear disk inventory using the nine requested disk metadata columns.
- Navigate from an activated disk inventory row into that disk's stored folder/file contents.
- Keep folder/file locations on their existing content-oriented columns and navigation behavior.
- Fetch root disk rows through the database using persisted metadata and paging.

**Non-Goals:**
- Editing disk metadata, changing disk ordering/sorting, or introducing column-configuration UI.
- Changing the `disks`, `folders`, `files`, or scan-statistics schema.
- Showing disk-inventory columns at folder/file locations or probing connected media for refreshed values.

## Decisions

- Treat `BrowserLocation::isRoot` as a right-pane display-mode boundary. A root location binds the list to disk inventory rows; any disk or descendant folder location binds it to file/folder content rows. Alternative considered: add a separate permanent disk pane, rejected because root selection already represents the catalog overview and one right pane preserves the browser layout.
- Configure the existing native owner-data `ListView` columns when location mode changes. Root mode installs, in order, `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`; contents mode restores the existing file/folder columns. Alternative considered: keep the five generic columns and squeeze disk fields into them, rejected because it neither presents the requested inventory nor keeps data meanings clear.
- Add storage reads that return paged `wit::core::Disk` records for root display, ordered deterministically by disk name with disk identity as a tie-breaker, while leaving folder/file page queries in their existing path. The UI adapter keeps mutually exclusive disk and file/folder page caches based on the bound mode. Alternative considered: continue manufacturing `FileEntry` objects for disks, rejected because it drops most disk fields and conflates disk activation with folder activation.
- Activate a root disk row using its stable disk ID and stored source/root path to form the existing non-root `BrowserLocation`, then reuse current navigation history, tree selection, address rendering, and child-content reads. Alternative considered: introduce a second navigation identity for disks, rejected because tree disk nodes already identify the same stored content root.
- Format capacity and free-space fields with the existing human-readable size formatting convention, render persisted `updated_at` with the application's current display-date convention, and render persisted disk-type enumeration values as media type labels without live device inspection. Alternative considered: refresh disk values on root display, rejected because browsing must remain available offline and not modify catalog state.

## Risks / Trade-offs

- [Older or incomplete disk metadata yields blank/default cells] -> Display persisted empty/default values consistently rather than inventing or probing for values; scanning and metadata editing can improve records separately.
- [Switching list columns while owner-data notifications arrive could expose stale display text] -> Rebind mode, clear the inactive page cache, update columns, reset item count, and invalidate the list as one navigation operation.
- [Catalog root history restores a different visual mode than disk/folder history] -> Keep mode derived solely from the restored `BrowserLocation`, so Back and Forward naturally reinstall the appropriate columns.
- [Many disk records could make the overview expensive] -> Retain count-plus-page queries and owner-data list population for root mode.

## Migration Plan

1. Add disk count/page access for catalog-root inventory using existing `disks` metadata.
2. Extend the right-pane view binding to support disk rows and root-specific columns alongside current content rows.
3. Wire disk activation into existing browser navigation and refresh/status behavior.
4. Update UI/storage documentation as needed, build Release x64, and manually verify both view modes and Back/Forward transitions.

No database migration is required. Rollback restores the generic root projection and contents columns without changing stored catalog files.

## Open Questions

None block implementation. Sort controls and editable disk metadata remain future behavior.
