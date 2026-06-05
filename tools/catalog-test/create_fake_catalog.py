#!/usr/bin/env python3
"""Create a large fake WhereIsThat catalog for UI/search performance testing."""

from __future__ import annotations

import argparse
import sqlite3
import time
from pathlib import Path


DEFAULT_FILE_COUNT = 500_000
DEFAULT_EXTENSIONS = ("txt", "jpg", "pdf", "zip", "mp3", "cpp", "sqlite", "docx")
SOURCE_PATH = r"X:\FakeSearchStressDisk"
BATCH_SIZE = 10_000
TABLE_SQL_FILES = (
    "catalog_metadata.sql",
    "disk_groups.sql",
    "disks.sql",
    "disk_scan_statistics.sql",
    "folders.sql",
    "files.sql",
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def read_sql(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def split_extensions(value: str) -> tuple[str, ...]:
    extensions = []
    for raw in value.split(","):
        extension = raw.strip().lower().lstrip(".")
        if extension:
            extensions.append(extension)
    if not extensions:
        raise argparse.ArgumentTypeError("at least one extension is required")
    return tuple(extensions)


def create_schema(connection: sqlite3.Connection, sql_dir: Path) -> None:
    connection.executescript(read_sql(sql_dir / "pragmas.sql"))
    for sql_file in TABLE_SQL_FILES:
        connection.executescript(read_sql(sql_dir / "tables" / sql_file))
    connection.execute("INSERT OR IGNORE INTO catalog_metadata(id, description) VALUES(1, '')")


def create_indexes(connection: sqlite3.Connection, sql_dir: Path) -> None:
    connection.executescript(read_sql(sql_dir / "indexes.sql"))


def use_bulk_load_pragmas(connection: sqlite3.Connection) -> None:
    connection.execute("PRAGMA journal_mode=OFF")
    connection.execute("PRAGMA synchronous=OFF")
    connection.execute("PRAGMA temp_store=MEMORY")


def insert_disk(connection: sqlite3.Connection, file_count: int, now: int) -> int:
    total_capacity = max(file_count, 1) * 8192
    free_space = total_capacity // 4
    cursor = connection.execute(
        """
        INSERT INTO disks(
            disk_group_id, disk_name, disk_number, source_path, volume_label,
            total_capacity, free_space, cluster_size, serial_number, file_system,
            total_files, total_folders, added_at, updated_at, description,
            category, location, disk_type
        )
        VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            "Fake Search Stress Disk",
            1,
            SOURCE_PATH,
            "FAKE_STRESS",
            total_capacity,
            free_space,
            4096,
            "FAKE-0001",
            "FAKEFS",
            file_count,
            1,
            now,
            now,
            "Synthetic catalog for search result scrolling tests.",
            "Test",
            "Generated",
            "VirtualDisk",
        ),
    )
    return int(cursor.lastrowid)


def insert_root_folder(connection: sqlite3.Connection, disk_id: int, now: int) -> int:
    cursor = connection.execute(
        """
        INSERT INTO folders(
            disk_id, parent_folder_id, path, name, created_at, modified_at,
            accessed_at, attributes, content_size, entry_type
        )
        VALUES(?, NULL, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (disk_id, SOURCE_PATH, "FakeSearchStressDisk", now, now, now, 16, 0, "directory"),
    )
    return int(cursor.lastrowid)


def file_rows(
    disk_id: int,
    folder_id: int,
    count: int,
    extensions: tuple[str, ...],
    now: int,
):
    extension_count = len(extensions)
    for index in range(1, count + 1):
        extension = extensions[(index - 1) % extension_count]
        name = f"search_test_file_{index:06d}.{extension}"
        size = 1024 + (index % 1_048_576)
        timestamp = now - (index % 31_536_000)
        yield (
            disk_id,
            folder_id,
            name,
            "",
            extension,
            None,
            size,
            timestamp,
            timestamp,
            timestamp,
            32,
        )


def insert_files(
    connection: sqlite3.Connection,
    disk_id: int,
    folder_id: int,
    file_count: int,
    extensions: tuple[str, ...],
    now: int,
) -> None:
    sql = """
        INSERT INTO files(
            disk_id, folder_id, name, description, extension, crc, size,
            created_at, modified_at, accessed_at, attributes
        )
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """
    batch = []
    for row in file_rows(disk_id, folder_id, file_count, extensions, now):
        batch.append(row)
        if len(batch) >= BATCH_SIZE:
            connection.executemany(sql, batch)
            batch.clear()
    if batch:
        connection.executemany(sql, batch)


def finalize_catalog(connection: sqlite3.Connection, disk_id: int, file_count: int, now: int) -> None:
    connection.execute(
        """
        INSERT INTO disk_scan_statistics(
            disk_id, last_scanned_at, image_scanning_time_ms,
            imported_descriptions_count, calculated_file_crcs, scanned_archives,
            archive_files_count, archive_folders_count
        )
        VALUES(?, ?, 0, 0, 0, 0, 0, 0)
        """,
        (disk_id, now),
    )
    connection.execute(
        "UPDATE folders SET content_size=(SELECT COALESCE(SUM(size), 0) FROM files WHERE folder_id=folders.id)"
    )
    connection.execute(
        "UPDATE disks SET total_files=?, total_folders=1, updated_at=? WHERE id=?",
        (file_count, now, disk_id),
    )


def verify_catalog(connection: sqlite3.Connection) -> None:
    integrity = connection.execute("PRAGMA integrity_check").fetchone()
    if not integrity or integrity[0] != "ok":
        raise RuntimeError(f"SQLite integrity check failed: {integrity[0] if integrity else 'no result'}")

    foreign_key_errors = connection.execute("PRAGMA foreign_key_check").fetchall()
    if foreign_key_errors:
        raise RuntimeError(f"foreign key check failed: {foreign_key_errors[:3]}")


def parse_args() -> argparse.Namespace:
    default_output = Path(__file__).with_name("fake-search-catalog.sqlite")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=default_output,
        help=f"catalog path to create; default: {default_output}",
    )
    parser.add_argument(
        "--file-count",
        type=int,
        default=DEFAULT_FILE_COUNT,
        help=f"number of files to generate; default: {DEFAULT_FILE_COUNT}",
    )
    parser.add_argument(
        "--extensions",
        type=split_extensions,
        default=DEFAULT_EXTENSIONS,
        help="comma-separated file extensions; default: "
        + ",".join(DEFAULT_EXTENSIONS),
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace the output catalog if it already exists",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.file_count < 0:
        raise SystemExit("--file-count must be 0 or greater")

    output = args.output.resolve()
    if output.exists() and not args.overwrite:
        raise SystemExit(f"{output} already exists; pass --overwrite to replace it")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    sql_dir = repo_root() / "sql"
    now = int(time.time())

    with sqlite3.connect(output, isolation_level=None) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        use_bulk_load_pragmas(connection)

        create_schema(connection, sql_dir)
        use_bulk_load_pragmas(connection)
        connection.execute("BEGIN")
        try:
            disk_id = insert_disk(connection, args.file_count, now)
            folder_id = insert_root_folder(connection, disk_id, now)
            insert_files(connection, disk_id, folder_id, args.file_count, args.extensions, now)
            finalize_catalog(connection, disk_id, args.file_count, now)
            connection.execute("COMMIT")
        except Exception:
            connection.execute("ROLLBACK")
            raise

        create_indexes(connection, sql_dir)
        connection.execute("PRAGMA journal_mode=DELETE")
        verify_catalog(connection)
        connection.execute("VACUUM")

    print(f"Created {output}")
    print(f"Files: {args.file_count}")
    print(f"Extensions: {', '.join(args.extensions)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
