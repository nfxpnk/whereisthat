## Why

Selecting the catalog root currently shows its indexed disks as folder-like entries using file-content columns, which hides the stored disk metadata users need at the catalog overview level. The root selection should instead provide a dedicated disk inventory view before the user navigates into an individual disk's contents.

## What Changes

- **BREAKING** Replace the active catalog root's right-pane contents presentation with a disks view rather than the ordinary folder/file list presentation.
- Display one row per indexed disk at the catalog root with columns, in order: `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`.
- Populate catalog-root rows from persisted disk metadata and keep them available for offline browsing without probing mounted media.
- Preserve folder/file content browsing for selected disk and descendant-folder locations, and allow activation of a disk in the root disks view to navigate into that disk.
- Preserve list paging behavior for catalogs containing many disks.

## Capabilities

### New Capabilities

### Modified Capabilities

- `catalog-content-browser`: Changes the right-pane behavior for catalog-root selection from a generic first-level contents listing to a dedicated disks inventory view with disk metadata columns and disk-to-content navigation.

## Impact

- Right-pane list presentation and mode-aware column configuration in `src/ui` and `src/app`.
- Storage-layer retrieval of paged `Disk` metadata records in `src/storage`, using the existing normalized `disks` table.
- Catalog browser interaction and verification documentation; no database schema migration is required.
