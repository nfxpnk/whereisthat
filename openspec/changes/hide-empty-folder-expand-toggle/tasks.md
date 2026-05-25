## 1. Stored Child-Folder Detection

- [x] 1.1 Add a prepared storage-layer presence query for whether a stored source/folder location has at least one immediate child folder, using normalized folder relationships and no live filesystem access.

## 2. Tree Marker Presentation

- [x] 2.1 Update catalog tree reload behavior so each indexed source-root node receives an expand toggle only when the stored presence query reports child folders.
- [x] 2.2 Update lazy descendant expansion so each newly inserted folder node receives an accurate expand toggle while leaf folders remain selectable for right-pane browsing.

## 3. Verification

- [x] 3.1 Build `whereisthat.sln` for `Release|x64`.
- [ ] 3.2 Manually verify that the catalog root exposes indexed sources, folders with subfolders display and perform expansion, and source or nested leaf folders, including folders containing files only, display no expand toggle while remaining browsable.
