CREATE TABLE IF NOT EXISTS disk_scan_statistics (
    disk_id INTEGER PRIMARY KEY,
    last_scanned_at INTEGER NOT NULL,
    image_scanning_time_ms INTEGER NOT NULL DEFAULT 0,
    imported_descriptions_count INTEGER NOT NULL DEFAULT 0,
    calculated_file_crcs INTEGER NOT NULL DEFAULT 0 CHECK (calculated_file_crcs IN (0, 1)),
    scanned_archives INTEGER NOT NULL DEFAULT 0 CHECK (scanned_archives >= 0),
    archive_files_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_files_count >= 0),
    archive_folders_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_folders_count >= 0),
    FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE
);
