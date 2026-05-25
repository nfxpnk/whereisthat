## Context

The implemented catalog browser owns a native `WC_TREEVIEW` control (`IDC_BROWSER_TREE`) whose selection changes navigate the right pane through `MainFrame::OnTreeSelectionChanged()`. `MainFrame` currently handles tree selection and expansion notifications but not right-click notifications. It already uses `TrackPopupMenuEx()` for a toolbar dropdown and centralizes implemented command behavior in `MainFrame::OnCommand()`.

Several requested context-menu labels already have resource command IDs. Implemented routes currently include `ID_EDIT_ADDDISKIMAGE`, `ID_FILE_NEWCATALOG`, `ID_FILE_OPEN`, and `ID_FILE_SAVE`; IDs including find-in-catalog, edit-description, rebuild/close, catalog-manager/setup, and properties are present in menu resources but are not currently routed to behavior. `Add New Disk Group` and `Update All Disk Images` have no existing command identifiers.

## Goals / Non-Goals

**Goals:**
- Show a native popup with the requested item order and separator groups only for a right-click hit on a valid tree item.
- Make the clicked item current before any context command can be chosen.
- Reuse established command IDs and `OnCommand()` routes for already implemented actions.
- Keep not-yet-implemented entries visible and inert with clear placeholder identity.
- Preserve existing tree navigation and existing main-menu, toolbar, and accelerator command behavior.

**Non-Goals:**
- Implement disk grouping, bulk disk updating, find-in-catalog, description editing, catalog rebuild/close/management/setup, or properties workflows.
- Introduce custom menu drawing, new menu icon artwork, storage changes, or per-tree-node command semantics beyond existing action guards.
- Redefine global menu availability policy where the current application does not expose one.

## Decisions

- Handle `NM_RCLICK` from `IDC_BROWSER_TREE` in `MainFrame`, convert the cursor point for `TreeView_HitTest`, and proceed only when the hit test identifies a real item. Select that `HTREEITEM` through `TreeView_SelectItem` before showing the popup, allowing the existing selection-change/navigation path to establish the command target. Alternative considered: show a popup for the currently selected item regardless of hit location, rejected because it violates the item-only interaction and could act on a different node than the user clicked.
- Build an ordinary native popup `HMENU` at invocation time with `CreatePopupMenu`, `AppendMenuW`, and `TrackPopupMenuEx(..., TPM_RETURNCMD, ...)`, then dispatch a returned command through the same `OnCommand()` entry point used by existing UI surfaces. Alternative considered: duplicate each action directly in the right-click handler, rejected because shared command dispatch is the existing reuse boundary.
- Reuse existing IDs for labels that already participate in the application's global command surface, including existing placeholder IDs that are currently inert. Add distinctly named IDs such as `ID_TREE_CONTEXT_ADD_NEW_DISK_GROUP_PLACEHOLDER` and `ID_TREE_CONTEXT_UPDATE_ALL_DISK_IMAGES_PLACEHOLDER` only for menu commands without any existing ID, and deliberately omit handlers for them with an explanatory comment near popup construction or command routing. Alternative considered: allocate context-only duplicates for all labels, rejected because it would disconnect implemented actions and create unnecessary future rewiring.
- Use the requested popup label text independently from main-menu wording: `Add New Disk Image` dispatches the existing Add/Update workflow and `Add New Catalog` dispatches existing New Catalog behavior. Alternative considered: change global menu labels to make them identical, rejected because this feature does not require changing established top-level menu presentation.
- Use standard disabled flags only when an existing reusable command-availability decision is available to the popup implementation. Current command guards can continue to reject unavailable invoked operations consistently with other entry points rather than introducing context-menu-only disabling rules. Alternative considered: infer broad node-specific enabling for placeholder or existing actions, rejected because the functionality and policy are not yet defined.
- Keep the popup a standard text menu. The application has toolbar imagery but no existing menu-icon attachment path; should a shared menu imagery mechanism be present when implemented, existing command imagery can be applied without adding placeholder assets. Alternative considered: owner-draw the new popup solely for icons, rejected as unnecessary UI infrastructure.

## Risks / Trade-offs

- [A right-click selection triggers ordinary navigation before the popup appears] -> Treat this as required selection behavior and reuse the existing selection notification flow so command context and visible browser location remain aligned.
- [Existing IDs are visible in the popup although their handlers are not yet implemented] -> Retain them as intentional no-op placeholders and mark the intentional omission clearly in code and verification.
- [Implemented commands may remain selectable when their guard will reject them] -> Preserve the application's existing behavior unless an existing availability state can be applied directly, avoiding inconsistent new policy.
- [Adding notification handling could interfere with tree expansion or left-click navigation] -> Add only the right-click branch and smoke test selection, expansion, and normal navigation after implementation.

## Migration Plan

No persisted data or schema migration is required. Add resource IDs only where absent, implement tree right-click popup creation/dispatch in the main frame, build the existing Release x64 target, and smoke test both context and existing entry points. Rollback removes the popup notification branch and the two context-only placeholder IDs without affecting saved catalogs.

## Open Questions

None block implementation. Future features can replace individual placeholder routes with shared action handlers and define finer-grained node-specific enablement.
