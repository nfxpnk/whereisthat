## Why

The catalog folder tree currently displays an expand toggle for every indexed folder, including folders that contain no stored child folders. Users can attempt to expand a leaf only to see nothing appear, which makes the offline tree look incomplete or misleading.

## What Changes

- Show an expand toggle in the catalog tree only for the catalog root or stored folder nodes that actually have child folder nodes to reveal.
- Keep leaf folders selectable and browsable in the contents pane without presenting an expansion action that cannot produce children.
- Preserve lazy tree loading and offline database-backed browsing while deriving expandability from stored folder relationships.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `catalog-content-browser`: Refine hierarchical folder tree presentation so leaf folders do not display an expand toggle.

## Impact

- Native tree-node population and child-marker behavior in `src/ui/CatalogTreeView.cpp`.
- Storage-backed child-folder discovery already used by tree expansion, potentially requiring a lightweight presence check or equivalent query use when inserting folder nodes.
- Focused validation for source roots, nested non-empty folders, and leaf folders in the left catalog tree.
