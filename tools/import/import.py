#!/usr/bin/env python3
"""Import a WhereIsIt XML report into the current Where Is That? SQLite format.

Usage:
    python import.py --xml catalog.xml --db catalog.db --overwrite --verbose
    python import.py --overwrite --verbose

The importer uses only the Python standard library. It writes the database to a
temporary file first and replaces the requested output only after the full import,
schema validation, and SQLite integrity checks succeed.
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import shutil
import sqlite3
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import Counter
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path, PureWindowsPath
from typing import Iterable


LOG = logging.getLogger("import_xml")
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_XML_PATH = SCRIPT_DIR / "catalog.xml"
DEFAULT_DB_PATH = SCRIPT_DIR / "catalog.db"
SQL_ROOT = PROJECT_ROOT / "sql"
MOJIBAKE_CHARS_RE = re.compile(r"[\u00C0-\u00FF\u00A8\u00B8\u201A-\u201E\u2020-\u2026\u2030\u2039\u203A\u20AC\u2122]")
CYRILLIC_RE = re.compile(r"[\u0400-\u04FF]")
SCHEMA_TABLE_FILES = [
    "catalog_metadata.sql",
    "disk_groups.sql",
    "disks.sql",
    "disk_scan_statistics.sql",
    "folders.sql",
    "files.sql",
]

REQUIRED_COLUMNS = {
    "catalog_metadata": {"description"},
    "disk_groups": {"name", "updated_at"},
    "disks": {"disk_group_id", "disk_name", "source_path", "disk_type"},
    "disk_scan_statistics": {
        "last_scanned_at",
        "scanned_archives",
        "archive_files_count",
        "archive_folders_count",
    },
    "folders": {"parent_folder_id", "path", "content_size", "entry_type"},
    "files": {"folder_id", "extension", "crc", "accessed_at"},
}

DISK_TYPE_MAP = {
    "audio cd": "CD",
    "blu-ray": "BluRay",
    "blu ray": "BluRay",
    "bd-rom": "BluRay",
    "cd": "CD",
    "cd-rom": "CD",
    "dvd": "DVD",
    "dvd-rom": "DVD",
    "hard disk": "HardDisk",
    "hdd": "HardDisk",
    "removable disk": "RemovableUSB",
    "usb": "RemovableUSB",
    "virtual disk": "VirtualDisk",
    "virtual hard disk": "VirtualDisk",
}


class ImportErrorWithContext(RuntimeError):
    """Raised for user-fixable import failures."""


@dataclass
class XmlItem:
    index: int
    item_type: str
    fields: dict[str, str]


@dataclass
class DiskRecord:
    xml_index: int
    name: str
    disk_number: int
    source_path: str
    disk_type: str
    total_capacity: int
    timestamp: int
    description: str
    category: str
    location: str
    xml_disk_type: str


@dataclass
class FolderRecord:
    disk_key: tuple[str, int]
    parent_path: str
    path: str
    name: str
    timestamp: int
    content_size: int
    entry_type: str
    xml_index: int
    implicit: bool = False


@dataclass
class FileRecord:
    disk_key: tuple[str, int]
    folder_path: str
    name: str
    description: str
    extension: str
    crc: str | None
    size: int
    timestamp: int
    xml_index: int


@dataclass
class ImportPlan:
    catalog_description: str
    disk_groups: list[XmlItem] = field(default_factory=list)
    disks: list[DiskRecord] = field(default_factory=list)
    folders: list[FolderRecord] = field(default_factory=list)
    files: list[FileRecord] = field(default_factory=list)
    xml_counts: Counter[str] = field(default_factory=Counter)
    warnings: Counter[str] = field(default_factory=Counter)
    unmapped_fields: Counter[str] = field(default_factory=Counter)


@dataclass
class ImportSummary:
    xml_counts: Counter[str]
    inserted: Counter[str]
    warnings: Counter[str]
    unmapped_fields: Counter[str]
    output_path: Path | None


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import a WhereIsIt XML report into a Where Is That? SQLite database."
    )
    parser.add_argument("--xml", type=Path, default=DEFAULT_XML_PATH, help="Input XML path.")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB_PATH, help="Output SQLite path.")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Allow replacing an existing output database.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and validate without writing a database.",
    )
    parser.add_argument(
        "--fix-xml-output",
        type=Path,
        help="Write a UTF-8 XML copy with repaired Cyrillic mojibake and exit.",
    )
    parser.add_argument(
        "--mojibake-demo",
        action="store_true",
        help="Run a small Cyrillic mojibake repair demo and exit.",
    )
    parser.add_argument("--verbose", action="store_true", help="Enable detailed logging.")
    return parser.parse_args(argv)


def configure_logging(verbose: bool) -> None:
    logging.basicConfig(
        level=logging.DEBUG if verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )


def text_of(item: ET.Element) -> dict[str, str]:
    result: dict[str, str] = {}
    for child in list(item):
        value = child.text if child.text is not None else ""
        result[child.tag] = value.strip()
    return result


def looks_like_cyrillic_mojibake(s: str) -> bool:
    if not s or s.isascii():
        return False
    mojibake_chars = MOJIBAKE_CHARS_RE.findall(s)
    if not mojibake_chars:
        return False
    if CYRILLIC_RE.search(s):
        return False
    return len(mojibake_chars) >= 2


def cyrillic_score(s: str) -> int:
    return len(CYRILLIC_RE.findall(s))


def cyrillic_ratio(s: str) -> float:
    letters = [char for char in s if char.isalpha()]
    if not letters:
        return 0.0
    return cyrillic_score(s) / len(letters)


def fix_cyrillic_mojibake(s: str) -> str:
    if not looks_like_cyrillic_mojibake(s):
        return s

    candidates: list[tuple[int, str, str]] = []
    for encoding in ("latin1", "cp1252"):
        try:
            repaired = s.encode(encoding).decode("cp1251")
        except UnicodeError as exc:
            LOG.warning("Could not repair suspected Cyrillic mojibake %r via %s: %s", s, encoding, exc)
            continue
        score = cyrillic_score(repaired)
        if score >= 3 and cyrillic_ratio(repaired) >= 0.5:
            candidates.append((score, encoding, repaired))

    if not candidates:
        LOG.warning("Could not repair suspected Cyrillic mojibake safely: %r", s)
        return s

    _, encoding, repaired = max(candidates, key=lambda candidate: candidate[0])
    if repaired != s:
        LOG.info("Repaired Cyrillic mojibake via %s: %r -> %r", encoding, s, repaired)
    return repaired


def repair_xml_text(value: str | None, context: str) -> str | None:
    if value is None:
        return None
    repaired = fix_cyrillic_mojibake(value)
    if repaired != value:
        LOG.info("Changed XML %s: %r -> %r", context, value, repaired)
    return repaired


def repair_xml_tree(root: ET.Element) -> None:
    def visit(element: ET.Element, path: str) -> None:
        for name, value in list(element.attrib.items()):
            element.attrib[name] = repair_xml_text(value, f"{path}@{name}") or ""
        element.text = repair_xml_text(element.text, f"{path} text")
        element.tail = repair_xml_text(element.tail, f"{path} tail")

        child_counts: Counter[str] = Counter()
        for child in list(element):
            child_name = xml_element_name(child)
            child_counts[child_name] += 1
            visit(child, f"{path}/{child_name}[{child_counts[child_name]}]")

    visit(root, root.tag)


def demo_cyrillic_mojibake_repair() -> None:
    samples = {
        "Êîìïàíèÿ": "Компания",
        "Âûáîð": "Выбор",
        "normal English text": "normal English text",
    }
    for broken, expected in samples.items():
        repaired = fix_cyrillic_mojibake(broken)
        status = "OK" if repaired == expected else "FAIL"
        LOG.info("Mojibake demo %s: %r -> %r", status, broken, repaired)


def required_text(item: XmlItem, field_name: str) -> str:
    value = item.fields.get(field_name, "").strip()
    if not value:
        raise ImportErrorWithContext(
            f"XML item #{item.index} ({item.item_type}) is missing required field {field_name}."
        )
    return value


def parse_int(item: XmlItem, field_name: str, default: int | None = None) -> int:
    value = item.fields.get(field_name, "").strip()
    if value == "":
        if default is not None:
            return default
        raise ImportErrorWithContext(
            f"XML item #{item.index} ({item.item_type}) is missing required integer {field_name}."
        )
    try:
        number = int(value)
    except ValueError as exc:
        raise ImportErrorWithContext(
            f"XML item #{item.index} ({item.item_type}) has invalid integer {field_name}={value!r}."
        ) from exc
    if number < 0:
        raise ImportErrorWithContext(
            f"XML item #{item.index} ({item.item_type}) has negative {field_name}={value!r}."
        )
    return number


def parse_timestamp(item: XmlItem) -> int:
    date_text = required_text(item, "DATE")
    time_text = item.fields.get("TIME", "00:00:00").strip() or "00:00:00"
    try:
        parsed = datetime.strptime(f"{date_text} {time_text}", "%Y-%m-%d %H:%M:%S")
    except ValueError as exc:
        raise ImportErrorWithContext(
            f"XML item #{item.index} ({item.item_type}) has invalid DATE/TIME "
            f"{date_text!r} {time_text!r}."
        ) from exc
    return int(parsed.replace(tzinfo=timezone.utc).timestamp())


def parse_child_timestamp(item: XmlItem, warnings: Counter[str]) -> int:
    if not item.fields.get("DATE", "").strip():
        warnings["child item timestamp defaulted because DATE is missing"] += 1
        return 0
    try:
        return parse_timestamp(item)
    except ImportErrorWithContext:
        warnings["child item timestamp defaulted because DATE/TIME is invalid"] += 1
        return 0


def map_disk_type(xml_value: str, warnings: Counter[str]) -> str:
    normalized = " ".join(xml_value.strip().lower().split())
    if normalized in DISK_TYPE_MAP:
        return DISK_TYPE_MAP[normalized]
    warnings[f"unknown disk type mapped to Other: {xml_value}"] += 1
    return "Other"


def disk_key(name: str, number: int) -> tuple[str, int]:
    return (name.casefold(), number)


def resolve_disk_key_by_number(
    item: XmlItem,
    disk_name: str,
    number: int,
    disks_by_key: dict[tuple[str, int], DiskRecord],
    disks_by_number: dict[int, list[tuple[str, int]]],
    warnings: Counter[str],
) -> tuple[str, int]:
    number_matches = disks_by_number.get(number, [])
    if len(number_matches) == 1:
        key = number_matches[0]
        if key != disk_key(disk_name, number):
            warnings["ignored child DISK_NAME and resolved disk by DISK_NUM"] += 1
        return key
    if len(number_matches) > 1:
        exact_key = disk_key(disk_name, number)
        if exact_key in disks_by_key:
            warnings["used child DISK_NAME to disambiguate duplicate DISK_NUM"] += 1
            return exact_key
        raise ImportErrorWithContext(
            f"XML item #{item.index} has ambiguous disk number "
            f"DISK_NUM={number}; child DISK_NAME={disk_name!r} did not match a unique disk."
        )
    raise ImportErrorWithContext(
        f"XML item #{item.index} references unknown disk number "
        f"DISK_NUM={number}; child DISK_NAME={disk_name!r} is ignored."
    )


def source_path_for(disk: XmlItem) -> str:
    name = required_text(disk, "NAME")
    number = parse_int(disk, "DISK_NUM", 0)
    # The XML report has disk-relative paths only, so create a stable synthetic
    # browse root. The app treats source_path as an opaque navigation identity.
    return f"\\\\WhereIsItImport\\Disk {number} - {name}"


def combine_stored_path(parent: str, name: str) -> str:
    if not parent:
        return name
    if parent.endswith(("\\", "/")):
        return parent + name
    return parent + "\\" + name


def xml_parent_to_stored_path(source_path: str, xml_path: str) -> str:
    cleaned = (xml_path or "\\").replace("/", "\\").strip()
    if cleaned in {"", "\\"}:
        return source_path
    cleaned = cleaned.lstrip("\\")
    return combine_stored_path(source_path, cleaned)


def display_name_with_extension(name: str, extension: str) -> str:
    extension = extension.strip().lstrip(".")
    if not extension:
        return name
    suffix = "." + extension
    return name if name.casefold().endswith(suffix.casefold()) else name + suffix


def normalize_extension(value: str) -> str:
    return value.strip().lstrip(".").casefold()


def normalize_crc(item: XmlItem) -> str | None:
    raw = item.fields.get("CRC", "").strip()
    if raw in {"", "0"}:
        return None
    return raw.upper()


def file_name_from_item(item: XmlItem, extension: str, warnings: Counter[str]) -> str | None:
    name = item.fields.get("NAME", "").strip()
    if name:
        return name
    if extension:
        warnings["file NAME defaulted from EXT"] += 1
        return "." + extension
    warnings["skipped file with missing NAME and EXT"] += 1
    return None


def collect_unmapped_fields(plan: ImportPlan, item: XmlItem, consumed: Iterable[str]) -> None:
    consumed_set = set(consumed)
    for field_name, value in item.fields.items():
        if field_name not in consumed_set and value.strip():
            plan.unmapped_fields[f"{item.item_type}.{field_name}"] += 1


def is_valid_xml_char(value: str) -> bool:
    codepoint = ord(value)
    return (
        codepoint == 0x09
        or codepoint == 0x0A
        or codepoint == 0x0D
        or 0x20 <= codepoint <= 0xD7FF
        or 0xE000 <= codepoint <= 0xFFFD
        or 0x10000 <= codepoint <= 0x10FFFF
    )


def xml_parser() -> ET.XMLParser:
    return ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))


def xml_element_name(element: ET.Element) -> str:
    return element.tag if isinstance(element.tag, str) else "comment()"


def declared_xml_encoding(raw: bytes) -> str | None:
    prefix = raw[:200].decode("ascii", errors="ignore")
    match = re.search(r"<\?xml[^>]*encoding=[\"']([^\"']+)[\"']", prefix, re.IGNORECASE)
    return match.group(1) if match else None


def sanitize_xml_text(text: str) -> tuple[str, int]:
    sanitized_chars: list[str] = []
    removed_control_chars = 0
    for value in text:
        if is_valid_xml_char(value):
            sanitized_chars.append(value)
        else:
            removed_control_chars += 1
    return "".join(sanitized_chars), removed_control_chars


def parse_xml_text(text: str) -> ET.ElementTree:
    return ET.ElementTree(ET.fromstring(text, parser=xml_parser()))


def sanitized_xml_tree_from_text(
    text: str,
    xml_path: Path,
    parse_error: ET.ParseError,
    invalid_unicode_replacements: int = 0,
) -> ET.ElementTree:
    sanitized, removed_control_chars = sanitize_xml_text(text)
    if not removed_control_chars and not invalid_unicode_replacements:
        raise ImportErrorWithContext(f"Malformed XML in {xml_path}: {parse_error}") from parse_error
    LOG.warning(
        "Retrying XML parse after removing %d invalid XML control character(s)"
        " and replacing %d invalid UTF-8 byte sequence(s).",
        removed_control_chars,
        invalid_unicode_replacements,
    )
    try:
        return parse_xml_text(sanitized)
    except ET.ParseError as exc:
        raise ImportErrorWithContext(
            f"Malformed XML in {xml_path} even after character sanitization: {exc}"
        ) from exc


def load_xml_tree(xml_path: Path) -> ET.ElementTree:
    raw = xml_path.read_bytes()
    try:
        return ET.ElementTree(ET.fromstring(raw, parser=xml_parser()))
    except ET.ParseError as normal_parse_error:
        declared_encoding = declared_xml_encoding(raw)
        if declared_encoding:
            try:
                declared_text = raw.decode(declared_encoding)
                try:
                    LOG.warning(
                        "Retrying XML parse after decoding raw bytes as declared encoding %s.",
                        declared_encoding,
                    )
                    return parse_xml_text(declared_text)
                except ET.ParseError:
                    return sanitized_xml_tree_from_text(declared_text, xml_path, normal_parse_error)
            except UnicodeError as exc:
                LOG.warning(
                    "Could not decode XML bytes using declared encoding %s: %s",
                    declared_encoding,
                    exc,
                )

        try:
            cp1251_text = raw.decode("cp1251")
            LOG.warning("Retrying XML parse after decoding raw bytes as cp1251.")
            try:
                return parse_xml_text(cp1251_text)
            except ET.ParseError:
                return sanitized_xml_tree_from_text(cp1251_text, xml_path, normal_parse_error)
        except UnicodeError as exc:
            LOG.warning("Could not decode XML bytes as cp1251: %s", exc)

        utf8_text = raw.decode("utf-8", errors="replace")
        return sanitized_xml_tree_from_text(
            utf8_text,
            xml_path,
            normal_parse_error,
            invalid_unicode_replacements=utf8_text.count("\ufffd"),
        )


def fix_xml_file(input_path: str, output_path: str) -> None:
    input_xml = Path(input_path)
    output_xml = Path(output_path)
    tree = load_xml_tree(input_xml)
    repair_xml_tree(tree.getroot())
    output_xml.parent.mkdir(parents=True, exist_ok=True)
    tree.write(output_xml, encoding="utf-8", xml_declaration=True)


def load_xml_items(xml_path: Path) -> tuple[str, list[XmlItem]]:
    if not xml_path.exists():
        raise ImportErrorWithContext(f"Input XML does not exist: {xml_path}")
    tree = load_xml_tree(xml_path)
    root = tree.getroot()
    repair_xml_tree(root)
    if root.tag != "REPORT":
        raise ImportErrorWithContext(f"Expected XML root <REPORT>, found <{root.tag}>.")
    title = root.attrib.get("Title", "").strip()
    items = [
        XmlItem(index=index, item_type=element.attrib.get("ItemType", "").strip(), fields=text_of(element))
        for index, element in enumerate(root.findall("ITEM"), start=1)
    ]
    if not items:
        raise ImportErrorWithContext("The XML report contains no ITEM records.")
    return title, items


def build_import_plan(xml_path: Path) -> ImportPlan:
    title, items = load_xml_items(xml_path)
    plan = ImportPlan(catalog_description=title)
    disks_by_key: dict[tuple[str, int], DiskRecord] = {}
    disks_by_number: dict[int, list[tuple[str, int]]] = {}

    for item in items:
        plan.xml_counts[item.item_type or "<missing>"] += 1
        if item.item_type == "DiskGroup":
            required_text(item, "NAME")
            parse_timestamp(item)
            plan.disk_groups.append(item)
            collect_unmapped_fields(
                plan,
                item,
                {"NAME", "DATE", "TIME", "DISK_TYPE"},
            )
        elif item.item_type == "Disk":
            name = required_text(item, "NAME")
            number = parse_int(item, "DISK_NUM", 0)
            timestamp = parse_timestamp(item)
            record = DiskRecord(
                xml_index=item.index,
                name=name,
                disk_number=number,
                source_path=source_path_for(item),
                disk_type=map_disk_type(required_text(item, "DISK_TYPE"), plan.warnings),
                total_capacity=parse_int(item, "SIZE", 0),
                timestamp=timestamp,
                description=item.fields.get("DESCRIPTION", ""),
                category=item.fields.get("CATEGORY", ""),
                location=item.fields.get("DISK_LOCATION", ""),
                xml_disk_type=item.fields.get("DISK_TYPE", ""),
            )
            key = disk_key(name, number)
            if key in disks_by_key:
                raise ImportErrorWithContext(
                    f"Duplicate disk identity in XML: NAME={name!r}, DISK_NUM={number}."
                )
            disks_by_key[key] = record
            disks_by_number.setdefault(number, []).append(key)
            plan.disks.append(record)
            collect_unmapped_fields(
                plan,
                item,
                {
                    "NAME",
                    "SIZE",
                    "DATE",
                    "DISK_TYPE",
                    "DESCRIPTION",
                    "DISK_NUM",
                    "TIME",
                    "CATEGORY",
                    "DISK_LOCATION",
                },
            )
        elif item.item_type in {"Folder", "File"}:
            disk_name = required_text(item, "DISK_NAME")
            number = parse_int(item, "DISK_NUM", 0)
            key = resolve_disk_key_by_number(
                item,
                disk_name,
                number,
                disks_by_key,
                disks_by_number,
                plan.warnings,
            )
            source_path = disks_by_key[key].source_path
            parent_path = xml_parent_to_stored_path(source_path, required_text(item, "PATH"))
            timestamp = parse_child_timestamp(item, plan.warnings)
            if item.item_type == "Folder":
                extension = normalize_extension(item.fields.get("EXT", ""))
                name = display_name_with_extension(required_text(item, "NAME"), extension)
                plan.folders.append(
                    FolderRecord(
                        disk_key=key,
                        parent_path=parent_path,
                        path=combine_stored_path(parent_path, name),
                        name=name,
                        timestamp=timestamp,
                        content_size=parse_int(item, "SIZE", 0),
                        entry_type="archive" if extension else "directory",
                        xml_index=item.index,
                    )
                )
                if extension:
                    plan.warnings["folder EXT imported by appending it to folder/archive name"] += 1
                collect_unmapped_fields(
                    plan,
                    item,
                    {
                        "NAME",
                        "EXT",
                        "SIZE",
                        "DATE",
                        "DISK_NAME",
                        "DISK_TYPE",
                        "PATH",
                        "DISK_NUM",
                        "TIME",
                        "CRC",
                    },
                )
            else:
                extension = normalize_extension(item.fields.get("EXT", ""))
                file_name = file_name_from_item(item, extension, plan.warnings)
                if file_name is None:
                    collect_unmapped_fields(
                        plan,
                        item,
                        {
                            "NAME",
                            "EXT",
                            "SIZE",
                            "DATE",
                            "DISK_NAME",
                            "DISK_TYPE",
                            "PATH",
                            "DESCRIPTION",
                            "DISK_NUM",
                            "TIME",
                            "CRC",
                        },
                    )
                    continue
                plan.files.append(
                    FileRecord(
                        disk_key=key,
                        folder_path=parent_path,
                        name=file_name,
                        description=item.fields.get("DESCRIPTION", ""),
                        extension=extension,
                        crc=normalize_crc(item),
                        size=parse_int(item, "SIZE", 0),
                        timestamp=timestamp,
                        xml_index=item.index,
                    )
                )
                collect_unmapped_fields(
                    plan,
                    item,
                    {
                        "NAME",
                        "EXT",
                        "SIZE",
                        "DATE",
                        "DISK_NAME",
                        "DISK_TYPE",
                        "PATH",
                        "DESCRIPTION",
                        "DISK_NUM",
                        "TIME",
                        "CRC",
                    },
                )
        else:
            raise ImportErrorWithContext(
                f"XML item #{item.index} has unsupported ItemType={item.item_type!r}."
            )

    if not plan.disks:
        raise ImportErrorWithContext("The XML report contains no Disk records.")

    add_root_and_missing_folders(plan)
    validate_plan(plan)
    return plan


def add_root_and_missing_folders(plan: ImportPlan) -> None:
    folders_by_disk_path: dict[tuple[tuple[str, int], str], FolderRecord] = {}
    additions: list[FolderRecord] = []

    for disk in plan.disks:
        key = disk_key(disk.name, disk.disk_number)
        root = FolderRecord(
            disk_key=key,
            parent_path="",
            path=disk.source_path,
            name=disk.name,
            timestamp=disk.timestamp,
            content_size=0,
            entry_type="directory",
            xml_index=disk.xml_index,
            implicit=True,
        )
        folders_by_disk_path[(key, root.path.casefold())] = root
        additions.append(root)

    for folder in plan.folders:
        dedupe_key = (folder.disk_key, folder.path.casefold())
        if dedupe_key in folders_by_disk_path:
            raise ImportErrorWithContext(
                f"Duplicate folder path derived from XML item #{folder.xml_index}: {folder.path!r}."
            )
        folders_by_disk_path[dedupe_key] = folder

    def ensure_parent(disk_key_value: tuple[str, int], path: str, timestamp: int) -> None:
        if not path:
            return
        dedupe_key = (disk_key_value, path.casefold())
        if dedupe_key in folders_by_disk_path:
            return
        parent = str(PureWindowsPath(path).parent)
        if parent == "." or parent == path:
            parent = ""
        ensure_parent(disk_key_value, parent, timestamp)
        name = PureWindowsPath(path).name or path
        implicit = FolderRecord(
            disk_key=disk_key_value,
            parent_path=parent,
            path=path,
            name=name,
            timestamp=timestamp,
            content_size=0,
            entry_type="directory",
            xml_index=0,
            implicit=True,
        )
        folders_by_disk_path[dedupe_key] = implicit
        additions.append(implicit)
        plan.warnings["created implicit parent folder"] += 1

    for folder in plan.folders:
        ensure_parent(folder.disk_key, folder.parent_path, folder.timestamp)
    for file in plan.files:
        ensure_parent(file.disk_key, file.folder_path, file.timestamp)

    plan.folders = additions + plan.folders


def validate_plan(plan: ImportPlan) -> None:
    disk_keys = {disk_key(d.name, d.disk_number) for d in plan.disks}
    folder_paths = {(folder.disk_key, folder.path.casefold()) for folder in plan.folders}
    for folder in plan.folders:
        if folder.disk_key not in disk_keys:
            raise ImportErrorWithContext(f"Folder references unknown disk: {folder.path!r}.")
        if folder.parent_path and (folder.disk_key, folder.parent_path.casefold()) not in folder_paths:
            raise ImportErrorWithContext(
                f"Folder {folder.path!r} is missing parent folder {folder.parent_path!r}."
            )
    for file in plan.files:
        if file.disk_key not in disk_keys:
            raise ImportErrorWithContext(f"File references unknown disk: {file.name!r}.")
        if (file.disk_key, file.folder_path.casefold()) not in folder_paths:
            raise ImportErrorWithContext(
                f"File XML item #{file.xml_index} references missing folder {file.folder_path!r}."
            )

    for field_name, count in plan.unmapped_fields.items():
        plan.warnings[f"unmapped XML field {field_name}"] += count


def create_schema(connection: sqlite3.Connection) -> None:
    connection.executescript((SQL_ROOT / "pragmas.sql").read_text(encoding="utf-8"))
    for file_name in SCHEMA_TABLE_FILES:
        connection.executescript((SQL_ROOT / "tables" / file_name).read_text(encoding="utf-8"))
    connection.execute("INSERT OR IGNORE INTO catalog_metadata(id,description) VALUES(1,'');")


def create_indexes(connection: sqlite3.Connection) -> None:
    statement_lines: list[str] = []
    for line in (SQL_ROOT / "indexes.sql").read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("--"):
            continue
        statement_lines.append(line)
        statement = "\n".join(statement_lines)
        if sqlite3.complete_statement(statement):
            connection.execute(statement)
            statement_lines.clear()
    if statement_lines:
        raise ImportErrorWithContext("indexes.sql ended with an incomplete SQL statement.")


def validate_schema(connection: sqlite3.Connection) -> None:
    for table, columns in REQUIRED_COLUMNS.items():
        actual = {row[1] for row in connection.execute(f"PRAGMA table_info({table});")}
        missing = columns - actual
        if missing:
            raise ImportErrorWithContext(
                f"Schema mismatch for {table}: missing columns {', '.join(sorted(missing))}."
            )


def import_to_database(plan: ImportPlan, db_path: Path) -> ImportSummary:
    inserted: Counter[str] = Counter()
    disk_id_by_key: dict[tuple[str, int], int] = {}
    folder_id_by_key_path: dict[tuple[tuple[str, int], str], int] = {}

    connection = sqlite3.connect(db_path, isolation_level=None)
    try:
        connection.execute("PRAGMA foreign_keys=ON;")
        create_schema(connection)
        validate_schema(connection)
        connection.execute("BEGIN;")
        try:
            connection.execute(
                "UPDATE catalog_metadata SET description=? WHERE id=1;",
                (plan.catalog_description,),
            )
            inserted["catalog_metadata"] = 1

            for group in plan.disk_groups:
                timestamp = parse_timestamp(group)
                connection.execute(
                    "INSERT INTO disk_groups(name,created_at,updated_at) VALUES(?,?,?);",
                    (required_text(group, "NAME"), timestamp, timestamp),
                )
                inserted["disk_groups"] += 1

            LOG.debug("Inserting %d disk row(s).", len(plan.disks))
            for disk in plan.disks:
                cursor = connection.execute(
                    "INSERT INTO disks("
                    "disk_group_id,disk_name,disk_number,source_path,volume_label,total_capacity,"
                    "free_space,cluster_size,serial_number,file_system,total_files,total_folders,"
                    "added_at,updated_at,description,category,location,disk_type"
                    ") VALUES(NULL,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                    (
                        disk.name,
                        disk.disk_number,
                        disk.source_path,
                        "",
                        disk.total_capacity,
                        0,
                        0,
                        "",
                        "",
                        0,
                        0,
                        disk.timestamp,
                        disk.timestamp,
                        disk.description,
                        disk.category,
                        disk.location,
                        disk.disk_type,
                    ),
                )
                disk_id = int(cursor.lastrowid)
                disk_id_by_key[disk_key(disk.name, disk.disk_number)] = disk_id
                inserted["disks"] += 1

            LOG.debug("Inserting %d folder row(s).", len(plan.folders))
            for folder in sorted(plan.folders, key=lambda f: (f.disk_key, f.path.count("\\"), f.path.casefold())):
                disk_id = disk_id_by_key[folder.disk_key]
                parent_id = None
                if folder.parent_path:
                    parent_id = folder_id_by_key_path[(folder.disk_key, folder.parent_path.casefold())]
                cursor = connection.execute(
                    "INSERT INTO folders("
                    "disk_id,parent_folder_id,path,name,created_at,modified_at,accessed_at,"
                    "attributes,content_size,entry_type"
                    ") VALUES(?,?,?,?,?,?,?,?,?,?);",
                    (
                        disk_id,
                        parent_id,
                        folder.path,
                        folder.name,
                        folder.timestamp,
                        folder.timestamp,
                        folder.timestamp,
                        0,
                        folder.content_size,
                        folder.entry_type,
                    ),
                )
                folder_id_by_key_path[(folder.disk_key, folder.path.casefold())] = int(cursor.lastrowid)
                inserted["folders"] += 1

            LOG.debug("Inserting %d file row(s).", len(plan.files))
            for file in plan.files:
                disk_id = disk_id_by_key[file.disk_key]
                folder_id = folder_id_by_key_path[(file.disk_key, file.folder_path.casefold())]
                connection.execute(
                    "INSERT INTO files("
                    "disk_id,folder_id,name,description,extension,crc,size,created_at,"
                    "modified_at,accessed_at,attributes"
                    ") VALUES(?,?,?,?,?,?,?,?,?,?,?);",
                    (
                        disk_id,
                        folder_id,
                        file.name,
                        file.description,
                        file.extension,
                        file.crc,
                        file.size,
                        file.timestamp,
                        file.timestamp,
                        file.timestamp,
                        0,
                    ),
                )
                inserted["files"] += 1

            LOG.debug("Creating indexes.")
            create_indexes(connection)
            LOG.debug("Updating rollups.")
            update_rollups(connection)
            LOG.debug("Inserting scan statistics.")
            inserted["disk_scan_statistics"] = insert_scan_statistics(connection)
            validate_schema(connection)
            LOG.debug("Checking database integrity.")
            check_database(connection)
            connection.execute("COMMIT;")
        except Exception:
            if connection.in_transaction:
                connection.execute("ROLLBACK;")
            raise
        finally:
            connection.execute("PRAGMA wal_checkpoint(TRUNCATE);")
    finally:
        connection.close()

    return ImportSummary(
        xml_counts=plan.xml_counts,
        inserted=inserted,
        warnings=plan.warnings,
        unmapped_fields=plan.unmapped_fields,
        output_path=db_path,
    )


def update_rollups(connection: sqlite3.Connection) -> None:
    folders = {
        int(folder_id): parent_folder_id
        for folder_id, parent_folder_id in connection.execute(
            "SELECT id,parent_folder_id FROM folders;"
        )
    }
    folder_sizes: Counter[int] = Counter()
    for folder_id, size in connection.execute("SELECT folder_id,size FROM files;"):
        current_id = int(folder_id)
        while current_id is not None:
            folder_sizes[current_id] += int(size)
            current_id = folders[current_id]
    connection.executemany(
        "UPDATE folders SET content_size=? WHERE id=? AND content_size=0;",
        ((size, folder_id) for folder_id, size in folder_sizes.items()),
    )
    connection.execute(
        "UPDATE disks SET "
        "total_files=(SELECT COUNT(*) FROM files WHERE files.disk_id=disks.id),"
        "total_folders=(SELECT COUNT(*) FROM folders WHERE folders.disk_id=disks.id),"
        "total_capacity=CASE "
        "WHEN total_capacity > 0 THEN total_capacity "
        "ELSE (SELECT COALESCE(SUM(size),0) FROM files WHERE files.disk_id=disks.id) "
        "END;"
    )


def insert_scan_statistics(connection: sqlite3.Connection) -> int:
    rows = list(
        connection.execute(
            "SELECT d.id,d.updated_at,"
            "(SELECT COUNT(*) FROM files f WHERE f.disk_id=d.id AND f.description <> ''),"
            "(SELECT COUNT(*) FROM files f WHERE f.disk_id=d.id AND f.crc IS NOT NULL),"
            "(SELECT COUNT(*) FROM folders fo WHERE fo.disk_id=d.id AND fo.entry_type='archive') "
            "FROM disks d;"
        )
    )
    for disk_id, updated_at, description_count, crc_count, archive_count in rows:
        connection.execute(
            "INSERT INTO disk_scan_statistics("
            "disk_id,last_scanned_at,image_scanning_time_ms,imported_descriptions_count,"
            "calculated_file_crcs,scanned_archives,archive_files_count,archive_folders_count"
            ") VALUES(?,?,?,?,?,?,?,?);",
            (
                disk_id,
                updated_at,
                0,
                description_count,
                1 if crc_count else 0,
                archive_count,
                0,
                archive_count,
            ),
        )
    return len(rows)


def check_database(connection: sqlite3.Connection) -> None:
    foreign_key_issues = list(connection.execute("PRAGMA foreign_key_check;"))
    if foreign_key_issues:
        raise ImportErrorWithContext(f"Foreign key check failed: {foreign_key_issues[:5]!r}")
    integrity = connection.execute("PRAGMA integrity_check;").fetchone()
    if not integrity or integrity[0] != "ok":
        raise ImportErrorWithContext(f"SQLite integrity check failed: {integrity!r}")


def sidecar_paths(path: Path) -> list[Path]:
    return [Path(str(path) + suffix) for suffix in ("-wal", "-shm", "-journal")]


def remove_file_if_exists(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def write_database_safely(plan: ImportPlan, output_path: Path, overwrite: bool) -> ImportSummary:
    output_path = output_path.resolve()
    if output_path.exists() and not overwrite:
        raise ImportErrorWithContext(
            f"Output database already exists: {output_path}. Pass --overwrite to replace it."
        )
    output_path.parent.mkdir(parents=True, exist_ok=True)

    temp_fd, temp_name = tempfile.mkstemp(
        prefix=output_path.stem + ".",
        suffix=".tmp.db",
        dir=output_path.parent,
    )
    os.close(temp_fd)
    temp_path = Path(temp_name)

    try:
        LOG.info("Writing temporary database: %s", temp_path)
        summary = import_to_database(plan, temp_path)
        for path in sidecar_paths(temp_path):
            remove_file_if_exists(path)
        if output_path.exists():
            remove_file_if_exists(output_path)
        for path in sidecar_paths(output_path):
            remove_file_if_exists(path)
        shutil.move(str(temp_path), str(output_path))
        summary.output_path = output_path
        LOG.info("Database written: %s", output_path)
        return summary
    except Exception:
        remove_file_if_exists(temp_path)
        for path in sidecar_paths(temp_path):
            remove_file_if_exists(path)
        raise


def print_summary(summary: ImportSummary) -> None:
    LOG.info("XML records parsed: %s", format_counter(summary.xml_counts))
    LOG.info("Rows inserted: %s", format_counter(summary.inserted))
    if summary.warnings:
        LOG.warning("Warnings/defaults: %s", format_counter(summary.warnings))
    else:
        LOG.info("Warnings/defaults: none")
    if summary.output_path:
        LOG.info("Output database: %s", summary.output_path)


def format_counter(counter: Counter[str]) -> str:
    if not counter:
        return "none"
    return ", ".join(f"{key}={counter[key]}" for key in sorted(counter))


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    configure_logging(args.verbose)

    try:
        if args.mojibake_demo:
            demo_cyrillic_mojibake_repair()
            return 0

        if args.fix_xml_output:
            LOG.info("Reading XML: %s", args.xml)
            fix_xml_file(str(args.xml), str(args.fix_xml_output))
            LOG.info("Fixed XML written: %s", args.fix_xml_output)
            return 0

        LOG.info("Reading XML: %s", args.xml)
        plan = build_import_plan(args.xml)
        LOG.info("Validated import plan for %d disks.", len(plan.disks))

        if args.dry_run:
            summary = ImportSummary(
                xml_counts=plan.xml_counts,
                inserted=Counter(),
                warnings=plan.warnings,
                unmapped_fields=plan.unmapped_fields,
                output_path=None,
            )
            LOG.info("Dry run complete; no database was written.")
            print_summary(summary)
            return 0

        summary = write_database_safely(plan, args.db, args.overwrite)
        print_summary(summary)
        return 0
    except ImportErrorWithContext as exc:
        LOG.error("%s", exc)
        return 2
    except sqlite3.Error as exc:
        LOG.error("SQLite import failed: %s", exc)
        return 3
    except OSError as exc:
        LOG.error("File operation failed: %s", exc)
        return 4


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
