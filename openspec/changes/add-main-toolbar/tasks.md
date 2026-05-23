## 1. Toolbar Resources And Artwork

- [x] 1.1 Select and copy only the semantically appropriate 16x16 transparent Fugue PNG files needed for the toolbar into the native app resource/assets area, verifying upstream filenames, original dimensions, transparency, and required attribution/license notice.
- [x] 1.2 Add toolbar command/resource identifiers, PNG resource entries, Visual Studio project entries, and any inbox Windows image-decoding link requirement needed to package and load the selected PNGs without scaling.

## 2. Native Toolbar Control

- [x] 2.1 Add main-frame toolbar/image-list ownership and create a Common Controls toolbar beneath the menu with 25x25 button sizing, 16x16 alpha-aware imagery, tooltips, required button order, and dividers.
- [x] 2.2 Route New, Open, Save, Report Generator, Search, Actions, and View toolbar buttons through the existing menu command IDs, adding only inert toolbar-specific sort IDs where no corresponding per-sort menu command exists.
- [x] 2.3 Implement the View Details split/dropdown arrow handling and native popup menu using the existing View List and View Details command identifiers.
- [x] 2.4 Update main-frame resize/display layout so catalog and file panes begin below the toolbar while existing status-bar behavior remains correct.

## 3. Shortcuts And Placeholder State

- [x] 3.1 Extend native accelerators for requested shared commands (`Ctrl+S`, `Ctrl+R`, `F3`, `Ctrl+E`, `Ctrl+P`, `Alt+Enter`, `F4`, and `Ctrl+I`) while retaining the existing `Ctrl+D` mapping to Add/Update Disk Image.
- [x] 3.2 Render the five sort buttons with pressed/unpressed-capable styles and an unpressed inert initial state, leaving a focused update path for synchronizing them once authoritative file-list sorting state exists.

## 4. Verification

- [x] 4.1 Build the supported x64 configuration and confirm toolbar PNG resources are present and display at 16x16 with preserved transparency inside 25x25 buttons.
- [x] 4.2 Smoke-test toolbar ordering, separators, tooltips, dropdown behavior, resizing, and existing New/Open/Search/Add-Update Disk Image/menu workflows, including `Ctrl+R` Report Generator dispatch, `F3` search, and the preserved `Ctrl+D` scan shortcut.
- [x] 4.3 Verify placeholder and sort toolbar actions do not modify catalog data, filesystem state, settings, result presentation, or item ordering until their matching features are implemented.

## 5. Manual Review Fixes

- [x] 5.1 Correct toolbar image-list/PNG rendering so transparent pixels retain the toolbar background instead of appearing black behind the Fugue icon artwork.
- [x] 5.2 Change all five sort placeholder buttons to preserve usable checked/unchecked visual state on click while remaining behaviorally inert with respect to file ordering and persisted settings.
- [ ] 5.3 Rebuild and manually verify transparent icon presentation and pressed/unpressed sort toggle behavior in the running toolbar.
