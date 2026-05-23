## Context

The main window is a Unicode Win32 frame in `src/app/MainFrame.cpp`. It currently creates two list views and an optional status bar, routes menu and accelerator activity through `WM_COMMAND` command IDs from `src/app/resource.h`, and defines its native menu and accelerators in `src/app/app.rc`. `ID_FILE_NEWCATALOG`, `ID_FILE_OPEN`, and `ID_SEARCH_FOR_ITEMS` already have active handlers; Save, Report Generator, View List/Details, and Actions command IDs exist as menu placeholders without behavior.

This change adds a presentation surface only. It must retain the existing command-routing boundary, fit the list-view layout beneath a newly occupied top row, use native Windows Common Controls, and package transparent 16x16 PNG assets sourced from the Fugue Icons repository. The new Report Generator shortcut is `Ctrl+R`, while the current accelerator table continues to assign `Ctrl+D` to `ID_EDIT_ADDDISKIMAGE`.

## Goals / Non-Goals

**Goals:**
- Create a native toolbar immediately beneath the menu with the required button order, dividers, sizes, tooltips, and image presentation.
- Dispatch matching toolbar buttons through existing command IDs so menu and toolbar behavior cannot diverge.
- Supply dropdown and toggle-style toolbar affordances that are ready for future view/sort implementations without fabricating current business behavior.
- Add only the needed transparent Fugue PNG assets and include them in the executable/build resources.
- Preserve existing active menu workflows and accelerator behavior.

**Non-Goals:**
- Implement Save, Report Generator, Actions operations, view switching, or sorting behavior that is currently absent from menu routing.
- Add persistence for toolbar visibility, selected view mode, or sort choices.
- Introduce a managed UI toolkit, third-party image loader, or non-Windows runtime dependency.

## Decisions

- Create the control as a native `TOOLBARCLASSNAMEW` child of `MainFrame`, with `TBSTYLE_FLAT`, tooltip support, and dropdown support. Set button size to 25x25 and image size to 16x16, allowing the native toolbar to center each image inside its button. Alternative considered: owner-drawing a custom strip, rejected because it duplicates standard toolbar interaction, accessibility, divider, and pressed-state behavior.
- Store the toolbar handle and any owned image-list/resource lifetime in the main-frame/UI ownership boundary, and update `OnSize` so the toolbar consumes the top portion of the client area before laying out catalog/file list views and the bottom status bar. Alternative considered: position the toolbar without adjusting content, rejected because it would overlap existing browsing controls.
- Give toolbar buttons corresponding to menu commands their existing command IDs: `ID_FILE_NEWCATALOG`, `ID_FILE_OPEN`, `ID_FILE_SAVE`, `ID_FILE_REPORT_GENERATOR`, `ID_SEARCH_FOR_ITEMS`, `ID_ACTIONS_EDIT_DESCRIPTION`, `ID_ACTIONS_PROPERTIES`, `ID_ACTIONS_OPEN_EXPLORER`, `ID_ACTIONS_VIEW_FILE`, `ID_VIEW_LIST`, and `ID_VIEW_DETAILS`. The toolbar therefore delivers ordinary `WM_COMMAND` messages through `MainFrame::OnCommand`; implemented handlers run unchanged and placeholder IDs remain inert. Alternative considered: toolbar-only callbacks, rejected because they would duplicate command routing and eventually drift from menus.
- Implement the View Details control using a dropdown toolbar button associated with `ID_VIEW_DETAILS`. A main-area click dispatches View Details; its arrow handles the toolbar dropdown notification and displays a native popup with `View List` and `View Details`, using `ID_VIEW_LIST` and `ID_VIEW_DETAILS`. Both entries retain their current inert status until view functionality is implemented. Alternative considered: assign new dropdown-only IDs, rejected because menu parity is required.
- Allocate toolbar-only command IDs for the five requested sort buttons unless a matching per-mode command is introduced before implementation. Render them with check-style/pressed-state-capable toolbar button styles and allow their local visual state to toggle while no file-list sorting behavior exists; they remain inert with respect to ordering and settings. Once sort state is defined, the toolbar shall reflect that authoritative state rather than storing a competing selection. Alternative considered: map every button to generic `ID_VIEW_SORT_ITEMS`, rejected because it cannot represent five distinct operations.
- Extend the accelerator resource for requested bindings, routing `Ctrl+S`, `Ctrl+R`, `F3`, `Ctrl+E`, `Ctrl+P`, `Alt+Enter`, `F4`, and `Ctrl+I` through the same command identifiers as their toolbar/menu actions; placeholder command IDs remain no-ops until their handlers exist. Continue to dispatch existing `Ctrl+N`, `Ctrl+O`, `Ctrl+D`, and `Ctrl+F` unchanged. Alternative considered: delay placeholder accelerators until command implementations exist, rejected because the requested command surface may safely dispatch inert shared IDs without inventing behavior.
- Select one semantically suitable existing Fugue 16x16 PNG for each displayed image during implementation, preferring the repository's document/database, open-folder, disk, report/document, magnifier, pencil, properties, folder, view/list, alphabetical/size/date sort, and reversed-order imagery. Store only the chosen PNGs in a toolbar asset/resource directory under `src/app`, list them in the Visual Studio project, and compile them as resource data. The Fugue file list includes native 16x16 action assets such as `application-list.png`, with selection finalized from the upstream set before copying. Alternative considered: rescaling larger artwork or embedding an icon sheet generated outside the project, rejected because source PNG size and transparency must be preserved.
- Decode PNG resource bytes through Windows Imaging Component or an equivalent inbox Windows API already compatible with native COM use. Composite decoded alpha pixels against the native toolbar face color as images are inserted into the Common Controls image list, preserving the intended transparent visual result on the application's supported native control path without a deployed image library. Alternative considered: convert assets to BMP for `TB_ADDBITMAP`, rejected because PNG sources must remain packaged and transparent.

## Risks / Trade-offs

- `[A new accelerator could accidentally disturb established shortcuts]` -> Add `Ctrl+R` for Report Generator and explicitly verify that existing `Ctrl+D` continues to invoke Add/Update Disk Image.
- `[Placeholder buttons appear available before their features exist]` -> Route them through the same currently inert IDs and do not change data, selection, file ordering, view state, or settings.
- `[PNG alpha can display incorrectly if passed directly through the native toolbar image list]` -> Decode PNG alpha with WIC, composite transparent pixels onto the toolbar face color at load time, and visually check icon edges against the toolbar background.
- `[Adding a top child control can reduce or overlap browse panes]` -> Measure the created toolbar height during layout and offset both list views while retaining current status-bar handling.
- `[Fugue asset naming may differ from illustrative icon names]` -> Choose and record only filenames that exist in the upstream repository during implementation; do not fabricate or upscale imagery.

## Migration Plan

Add the selected PNG files and native resource IDs, create and populate the toolbar in the main frame, integrate layout/notifications/accelerators, then build the x64 application and smoke-test existing menu operations plus toolbar interactions. No database or settings migration is required. Reversion removes the toolbar control and asset resource entries while leaving all existing menu functionality and user data unchanged.

## Open Questions

- When view and sort behavior is implemented, will its state be session-only or persisted as a display preference?
