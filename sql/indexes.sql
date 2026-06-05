CREATE INDEX IF NOT EXISTS idx_disk_groups_name
    ON disk_groups(name COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_disks_group
    ON disks(disk_group_id, disk_name COLLATE NOCASE);

CREATE UNIQUE INDEX IF NOT EXISTS idx_disks_source_path
    ON disks(source_path COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_folders_parent
    ON folders(disk_id, parent_folder_id);

CREATE INDEX IF NOT EXISTS idx_folders_parent_name
    ON folders(disk_id, parent_folder_id, name);

CREATE INDEX IF NOT EXISTS idx_folders_name
    ON folders(name);

CREATE UNIQUE INDEX IF NOT EXISTS idx_folders_disk_path
    ON folders(disk_id, path COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_files_folder
    ON files(folder_id);

CREATE INDEX IF NOT EXISTS idx_files_folder_name
    ON files(folder_id, name);

CREATE INDEX IF NOT EXISTS idx_files_name
    ON files(name);

CREATE INDEX IF NOT EXISTS idx_files_disk_name
    ON files(disk_id, name);

CREATE INDEX IF NOT EXISTS idx_files_extension
    ON files(extension);
