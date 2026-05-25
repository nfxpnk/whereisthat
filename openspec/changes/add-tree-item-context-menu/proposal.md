## Why

The catalog browser tree currently supports selection and expansion but gives users no item-scoped right-click command surface. A native context menu makes common catalog and disk actions discoverable at the selected tree location while preserving the application's existing command handling.

## What Changes

- Add a native Win32 context menu for valid catalog-tree items, opened by right-clicking an item after selecting that item.
- Present the requested command groups and separators in their specified order: disk-image/group operations, catalog-scoped find, description editing, catalog lifecycle actions, and catalog management/properties actions.
- Route menu entries with existing implemented behavior through the same command IDs and `MainFrame` command dispatch already used by menu, toolbar, or accelerator entry points.
- Keep entries without implemented application behavior visible as clearly identified placeholder commands that perform no action yet.
- Reflect existing availability/disabled state decisions in the popup wherever the application currently provides such command eligibility logic, without changing toolbar or main-menu behavior.
- Reuse existing command imagery when applicable and available; no new placeholder imagery is required.

## Capabilities

### New Capabilities
- `tree-item-context-menu`: Defines right-click selection, popup visibility and ordering, shared action routing, placeholder behavior, and command availability for catalog tree items.

### Modified Capabilities

## Impact

- `src/app/MainFrame.cpp`, `src/app/MainFrame.h`, and `src/app/resource.h` will gain native tree right-click handling, popup construction/dispatch, and any clearly named placeholder IDs not already defined.
- The existing `IDC_BROWSER_TREE` control and shared `WM_COMMAND` routing are reused; the tree's content population and normal navigation behavior are not replaced.
- Existing resource/menu/toolbar command identifiers and handlers are reused wherever functionality is already implemented, with no storage schema change or new dependency.
