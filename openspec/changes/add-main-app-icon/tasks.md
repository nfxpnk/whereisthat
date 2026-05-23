## 1. Icon Asset

- [x] 1.1 Create a multi-size Windows `.ico` asset for the Where Is That? main application icon.
- [x] 1.2 Place the icon asset in the app resource area and confirm the path is stable for the resource compiler.

## 2. Resource Wiring

- [x] 2.1 Add a dedicated app icon resource ID to `src/app/resource.h`.
- [x] 2.2 Register the `.ico` asset in `src/app/app.rc` using the new resource ID.
- [x] 2.3 Add the icon asset to `WhereIsThat.vcxproj` if needed for Visual Studio project visibility or build tracking.

## 3. Window Integration

- [x] 3.1 Load the app icon from the module instance in `MainFrame::Create()`.
- [x] 3.2 Set both `WNDCLASSEXW::hIcon` and `WNDCLASSEXW::hIconSm` to application-owned icon handles.
- [x] 3.3 Preserve the existing window creation, menu loading, and startup behavior.

## 4. Verification

- [x] 4.1 Build the project with MSBuild or Visual Studio to verify resource compilation succeeds.
- [x] 4.2 Run the app and confirm the title bar, taskbar, Alt-Tab, and executable shell icon show the Where Is That? icon.
- [x] 4.3 Smoke test existing startup, menu, catalog list, and file list behavior.
