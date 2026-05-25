## Context

`CatalogTreeView` lazily loads descendant folders from the active catalog database. At present `Reload` inserts every source root with `mayHaveChildren=true`, and `Expand` inserts every returned descendant the same way. The native TreeView therefore draws a plus/expand toggle on a stored leaf until that leaf is expanded once and its zero-child result corrects `cChildren`.

The normalized catalog schema already links folders with `parent_folder_id`, and browsing must remain database-backed and usable without the source media connected.

## Goals / Non-Goals

**Goals:**
- Accurately display an expand toggle only on tree nodes with stored child folders.
- Keep child node materialization lazy and continue using persisted catalog data.
- Handle both indexed source-root nodes and deeper descendant nodes consistently.

**Non-Goals:**
- Display files in the folder tree or change right-pane browsing.
- Change scan output, database schema, catalog navigation history, or tree styling.
- Preload the entire stored hierarchy.

## Decisions

- Add a storage read operation that reports whether a folder identified by source/disk ID and stored path has at least one immediate child folder. It can use an existence-style query over the indexed `folders(disk_id,parent_folder_id)` relationship and avoids consulting the live filesystem. Alternative considered: call `GetChildFolders` early and cache every child list; rejected because the tree only needs a boolean until expansion and ownership/caching would broaden this small correction.
- Compute `mayHaveChildren` before inserting each source-root or descendant folder node and pass that result through the existing `TVIF_CHILDREN` insertion path. A leaf is then born with `cChildren=0`, rather than showing an action that is corrected only after a failed expansion. Alternative considered: retain optimistic expand toggles and remove them on expansion; rejected because it is the reported misleading behavior.
- Retain current lazy population in `Expand`: an expandable folder still loads its immediate child nodes only on expansion, and each newly inserted child receives its own accurate marker from stored relationships. The catalog virtual root continues to use whether indexed sources exist, because its children are source roots rather than stored descendant folders.

## Risks / Trade-offs

- Checking expandability while adding visible child nodes adds small database reads during a tree expansion. -> Use a boolean existence query backed by the existing parent-folder index and run it only for nodes actually materialized in the tree.
- Catalog contents could be refreshed after markers were calculated. -> Preserve the existing reload/reset behavior after catalog or scan refresh so markers are rebuilt from current stored data.
- A source root with files but no child folders will no longer expand even though its contents pane is non-empty. -> This is intentional because the tree represents folders only; selection still displays those files in the contents pane.

## Migration Plan

1. Add the stored-child-folder presence read operation and use it when tree folder nodes are inserted.
2. Verify marker behavior for empty source roots, source roots with files only, and nested folders with and without subfolders.

No stored-data migration is required. Rolling back the UI/read operation leaves catalogs unchanged.

## Open Questions

None.
