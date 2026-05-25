CREATE UNIQUE INDEX IF NOT EXISTS idx_disks_source_path
    ON disks(source_path COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_folders_parent
    ON folders(disk_id, parent_folder_id);

CREATE UNIQUE INDEX IF NOT EXISTS idx_folders_disk_path
    ON folders(disk_id, path COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_files_folder
    ON files(folder_id);

CREATE INDEX IF NOT EXISTS idx_files_disk_name
    ON files(disk_id, name);

CREATE INDEX IF NOT EXISTS idx_files_extension
    ON files(extension);
