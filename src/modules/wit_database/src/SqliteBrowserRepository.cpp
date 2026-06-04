#include "wit_database/SqliteBrowserRepository.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <cstdint>
#include <string>

namespace wit::storage {
namespace {
std::wstring Text(sqlite3_stmt* stmt, int column) {
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return value ? wit::platform::ToUtf16(value) : std::wstring{};
}

void PopulateDisplayEntry(wit::core::FileEntry& entry, sqlite3_stmt* stmt) {
    entry.id = sqlite3_column_int64(stmt, 0);
    entry.catalogId = sqlite3_column_int64(stmt, 1);
    entry.parentPath = Text(stmt, 2);
    entry.name = Text(stmt, 3);
    entry.extension = Text(stmt, 4);
    entry.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    entry.modifiedAt = sqlite3_column_int64(stmt, 6);
    entry.attributes = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    entry.isDirectory = sqlite3_column_int(stmt, 8) != 0;
    entry.isArchive = Text(stmt, 9) == L"archive";
}

void PopulateBrowserDisk(wit::core::Disk& disk, sqlite3_stmt* stmt, int firstColumn) {
    disk.id = sqlite3_column_int64(stmt, firstColumn);
    disk.diskGroupId = sqlite3_column_type(stmt, firstColumn + 1) == SQLITE_NULL
        ? 0 : sqlite3_column_int64(stmt, firstColumn + 1);
    disk.diskName = Text(stmt, firstColumn + 2);
    disk.diskNumber = sqlite3_column_int64(stmt, firstColumn + 3);
    disk.sourcePath = Text(stmt, firstColumn + 4);
    disk.totalCapacity = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, firstColumn + 5));
    disk.freeSpace = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, firstColumn + 6));
    disk.updatedAt = sqlite3_column_int64(stmt, firstColumn + 7);
    disk.description = Text(stmt, firstColumn + 8);
    disk.category = Text(stmt, firstColumn + 9);
    disk.location = Text(stmt, firstColumn + 10);
    const auto type = Text(stmt, firstColumn + 11);
    if (type == L"CD") disk.diskType = wit::core::DiskType::CD;
    else if (type == L"DVD") disk.diskType = wit::core::DiskType::DVD;
    else if (type == L"BluRay") disk.diskType = wit::core::DiskType::BluRay;
    else if (type == L"HardDisk") disk.diskType = wit::core::DiskType::HardDisk;
    else if (type == L"SolidStateDisk") disk.diskType = wit::core::DiskType::SolidStateDisk;
    else if (type == L"RemovableUSB") disk.diskType = wit::core::DiskType::RemovableUSB;
    else if (type == L"VirtualDisk") disk.diskType = wit::core::DiskType::VirtualDisk;
    else disk.diskType = wit::core::DiskType::Other;
}
}

SqliteBrowserRepository::SqliteBrowserRepository(sqlite3* db) : db_(db) {}

void SqliteBrowserRepository::SetDatabase(sqlite3* db) {
    db_ = db;
}

int SqliteBrowserRepository::GetBrowserItemCount(const wit::core::BrowserLocation& location) {
    if (location.isRoot || location.isDiskGroup) return GetBrowserRootItemCount(location);
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT (SELECT COUNT(*) FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?) + "
        "(SELECT COUNT(*) FROM files f JOIN parent p ON f.folder_id=p.id);");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

int SqliteBrowserRepository::GetBrowserRootItemCount(const wit::core::BrowserLocation& location) {
    if (location.isDiskGroup) {
        SQLiteStatement statement(db_,
            "SELECT (SELECT COUNT(*) FROM disk_groups WHERE parent_group_id=?) + "
            "(SELECT COUNT(*) FROM disks WHERE disk_group_id=?);");
        statement.BindInt64(1, location.diskGroupId);
        statement.BindInt64(2, location.diskGroupId);
        return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
    }
    SQLiteStatement statement(db_,
        "SELECT (SELECT COUNT(*) FROM disk_groups WHERE parent_group_id IS NULL) + "
        "(SELECT COUNT(*) FROM disks WHERE disk_group_id IS NULL);");
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::BrowserItem> SqliteBrowserRepository::GetBrowserRootItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    std::vector<wit::core::BrowserItem> items;
    if (location.isDiskGroup) {
        SQLiteStatement statement(db_,
            "SELECT kind,id,disk_group_id,name,disk_number,source_path,total_capacity,free_space,updated_at,"
            "description,category,location,disk_type,total_disks,parent_group_id FROM ("
            "SELECT 0 AS kind,g.id AS id,NULL AS disk_group_id,g.name AS name,0 AS disk_number,'' AS source_path,"
            "0 AS total_capacity,0 AS free_space,g.updated_at AS updated_at,'' AS description,'' AS category,"
            "'' AS location,'Other' AS disk_type,COUNT(d.id) AS total_disks,g.parent_group_id AS parent_group_id "
            "FROM disk_groups g LEFT JOIN disks d ON d.disk_group_id=g.id WHERE g.parent_group_id=? "
            "GROUP BY g.id,g.name,g.updated_at,g.parent_group_id "
            "UNION ALL "
            "SELECT 1 AS kind,d.id,d.disk_group_id,d.disk_name,d.disk_number,d.source_path,d.total_capacity,"
            "d.free_space,d.updated_at,d.description,d.category,d.location,d.disk_type,0 AS total_disks,NULL AS parent_group_id "
            "FROM disks d WHERE d.disk_group_id=?) "
            "ORDER BY kind,name COLLATE NOCASE,id LIMIT ? OFFSET ?;");
        statement.BindInt64(1, location.diskGroupId);
        statement.BindInt64(2, location.diskGroupId);
        statement.BindInt64(3, limit);
        statement.BindInt64(4, offset);
        while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
            wit::core::BrowserItem item;
            if (sqlite3_column_int(statement.Raw(), 0) == 0) {
                item.type = wit::core::BrowserItemType::DiskGroup;
                item.group.id = sqlite3_column_int64(statement.Raw(), 1);
                item.group.parentGroupId = sqlite3_column_type(statement.Raw(), 14) == SQLITE_NULL
                    ? 0 : sqlite3_column_int64(statement.Raw(), 14);
                item.group.name = Text(statement.Raw(), 3);
                item.group.updatedAt = sqlite3_column_int64(statement.Raw(), 8);
                item.group.totalDisks = sqlite3_column_int64(statement.Raw(), 13);
            } else {
                item.type = wit::core::BrowserItemType::Disk;
                PopulateBrowserDisk(item.disk, statement.Raw(), 1);
            }
            items.push_back(item);
        }
        return items;
    }
    SQLiteStatement statement(db_,
        "SELECT kind,id,disk_group_id,name,disk_number,source_path,total_capacity,free_space,updated_at,"
        "description,category,location,disk_type,total_disks,parent_group_id FROM ("
        "SELECT 0 AS kind,g.id AS id,NULL AS disk_group_id,g.name AS name,0 AS disk_number,'' AS source_path,"
        "0 AS total_capacity,0 AS free_space,g.updated_at AS updated_at,'' AS description,'' AS category,"
        "'' AS location,'Other' AS disk_type,COUNT(d.id) AS total_disks,g.parent_group_id AS parent_group_id "
        "FROM disk_groups g LEFT JOIN disks d ON d.disk_group_id=g.id WHERE g.parent_group_id IS NULL "
        "GROUP BY g.id,g.name,g.updated_at,g.parent_group_id "
        "UNION ALL "
        "SELECT 1 AS kind,d.id,d.disk_group_id,d.disk_name,d.disk_number,d.source_path,d.total_capacity,"
        "d.free_space,d.updated_at,d.description,d.category,d.location,d.disk_type,0 AS total_disks,NULL AS parent_group_id "
        "FROM disks d WHERE d.disk_group_id IS NULL) "
        "ORDER BY kind,name COLLATE NOCASE,id LIMIT ? OFFSET ?;");
    statement.BindInt64(1, limit);
    statement.BindInt64(2, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::BrowserItem item;
        if (sqlite3_column_int(statement.Raw(), 0) == 0) {
            item.type = wit::core::BrowserItemType::DiskGroup;
            item.group.id = sqlite3_column_int64(statement.Raw(), 1);
            item.group.parentGroupId = sqlite3_column_type(statement.Raw(), 14) == SQLITE_NULL
                ? 0 : sqlite3_column_int64(statement.Raw(), 14);
            item.group.name = Text(statement.Raw(), 3);
            item.group.updatedAt = sqlite3_column_int64(statement.Raw(), 8);
            item.group.totalDisks = sqlite3_column_int64(statement.Raw(), 13);
        } else {
            item.type = wit::core::BrowserItemType::Disk;
            PopulateBrowserDisk(item.disk, statement.Raw(), 1);
        }
        items.push_back(item);
    }
    return items;
}

std::vector<wit::core::FileEntry> SqliteBrowserRepository::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    if (location.isRoot || location.isDiskGroup) return files;
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT c.id,c.disk_id,p.path AS parent_path,c.name,'' AS extension,c.content_size AS size,c.modified_at,c.attributes,1 AS is_directory,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=? "
        "UNION ALL SELECT f.id,f.disk_id,p.path AS parent_path,f.name,f.extension,f.size,f.modified_at,f.attributes,0 AS is_directory,'file' AS entry_type "
        "FROM files f JOIN parent p ON f.folder_id=p.id)"
        " ORDER BY is_directory DESC,name LIMIT ? OFFSET ?;");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    statement.BindInt64(4, limit);
    statement.BindInt64(5, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        PopulateDisplayEntry(file, statement.Raw());
        files.push_back(file);
    }
    return files;
}

bool SqliteBrowserRepository::HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) {
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT EXISTS(SELECT 1 FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?);");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW && sqlite3_column_int(statement.Raw(), 0) != 0;
}

std::vector<wit::core::FileEntry> SqliteBrowserRepository::GetChildFolders(
    std::int64_t sourceId, const std::wstring& parentPath) {
    std::vector<wit::core::FileEntry> folders;
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT c.id,c.disk_id,p.path,c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id "
        "WHERE c.disk_id=? ORDER BY c.name;");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry folder;
        PopulateDisplayEntry(folder, statement.Raw());
        folders.push_back(folder);
    }
    return folders;
}
}
