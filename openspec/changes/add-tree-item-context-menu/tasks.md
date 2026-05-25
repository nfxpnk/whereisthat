## 1. Command Surface And Popup Wiring

- [x] 1.1 Add clearly named resource command IDs for `Add New Disk Group` and `Update All Disk Images` context-menu placeholders while retaining existing IDs for already defined actions.
- [x] 1.2 Add `IDC_BROWSER_TREE` right-click notification handling in `MainFrame` that hit-tests the cursor, ignores empty tree space, and selects a valid clicked item before popup display.
- [x] 1.3 Construct the native popup menu with the required labels and separator order, returning the selected command into existing `MainFrame::OnCommand()` dispatch.

## 2. Shared Actions And Placeholders

- [x] 2.1 Map `Add New Disk Image`, `Add New Catalog`, `Open Catalog`, and `Save Catalog` entries to their existing command IDs and verify their current handlers and guards are reused unchanged.
- [x] 2.2 Use existing inert command IDs for deferred commands already defined in the global menu surface and leave the new context-only IDs unhandled, with a clear code comment documenting intentional placeholders.
- [x] 2.3 Apply any existing reusable disabled-state or menu-imagery behavior available for shared commands without introducing new placeholder actions or assets.

## 3. Verification

- [x] 3.1 Build the supported x64 configuration and resolve compile or resource-definition errors introduced by the tree context menu.
- [x] 3.2 Smoke-test right-clicking valid and empty tree locations, selection-before-menu behavior, exact menu ordering/separators, and ordinary tree selection/expansion navigation.
- [ ] 3.3 Smoke-test shared implemented entries through both context and existing command routes, and confirm every deferred entry is visible but inert.
