## 1. Contract And Database Format

- [x] 1.1 Update maintained storage, UI, architecture, and acceptance specifications to define persisted recursive folder content sizes, responsive stored-value browsing, and intentional rejection of normalized catalogs without the required field.
- [x] 1.2 Extend the canonical SQLite `folders` table definition, documentation inventory, and storage validation contract with required `content_size INTEGER NOT NULL DEFAULT 0`, without adding migration or backfill behavior.
- [x] 1.3 Update the database documentation verifier expectations and narrative pages for the newly required folder field and unchanged new-format-only rejection policy.

## 2. Scan-Time Folder Aggregate Persistence

- [x] 2.1 Extend the folder domain/storage model and prepared operations so folder rows can persist and update recursive `content_size` values within a staged transaction.
- [x] 2.2 Refactor `FileScanner` traversal to finalize each folder after stored descendants have been scanned, accumulating direct file and descendant byte totals only as transient scan state.
- [x] 2.3 Confirm successful add/rescan publishes folder totals with replacement content, disk totals, and latest statistics atomically, while cancellation or failure publishes no partial totals.

## 3. Responsive Contents Browsing

- [x] 3.1 Change mixed right-pane folder/file page projection to obtain folder sizes from persisted `folders.content_size` and display folder rows in the existing `Size` column without navigation-time recursive aggregation.
- [x] 3.2 Include displayed persisted folder sizes in focused-item and selected-item status calculations while preserving current disk-inventory status behavior.
- [x] 3.3 Refine count/page storage reads for selected disk or folder locations to resolve current folder identity through indexed paths and retrieve immediate children through parent/folder ownership indexes rather than disk-wide browse work.
- [x] 3.4 Keep page ordering owned by SQLite and document the persisted numeric folder/file size projection as the foundation for a later, separately specified list-view sorting feature.

## 4. Verification

- [x] 4.1 Extend storage smoke coverage with direct-file, nested-file, and empty-folder fixtures that verify persisted recursive totals and right-pane page projections.
- [x] 4.2 Add coverage that earlier normalized catalog fixtures lacking `folders.content_size` are rejected without alteration and that cancelled/failed staged scans do not expose aggregate data.
- [x] 4.3 Run database documentation verification and the storage smoke check, then build `whereisthat.sln` for `Release|x64`.
- [ ] 4.4 Manually verify disk/folder navigation displays stored folder sizes without the prior visible pause, remains browsable offline, and leaves currently placeholder sort controls behaviorally unchanged.
