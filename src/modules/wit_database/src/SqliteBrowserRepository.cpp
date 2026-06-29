#include "wit_database/SqliteBrowserRepository.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <Windows.h>
#include <algorithm>
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
    if (sqlite3_column_count(stmt) > 10) entry.fullPath = Text(stmt, 10);
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

void PopulateBrowserGroup(wit::core::DiskGroup& group, sqlite3_stmt* stmt) {
    group.id = sqlite3_column_int64(stmt, 1);
    group.parentGroupId = sqlite3_column_type(stmt, 14) == SQLITE_NULL ? 0 : sqlite3_column_int64(stmt, 14);
    group.name = Text(stmt, 3);
    group.totalCapacity = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
    group.freeSpace = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
    group.updatedAt = sqlite3_column_int64(stmt, 8);
    group.totalDisks = sqlite3_column_int64(stmt, 13);
}

int NaturalNoCaseCollation(void*, int leftBytes, const void* leftValue, int rightBytes, const void* rightValue) {
    const std::string leftUtf8(static_cast<const char*>(leftValue), static_cast<std::size_t>(leftBytes));
    const std::string rightUtf8(static_cast<const char*>(rightValue), static_cast<std::size_t>(rightBytes));
    const auto left = wit::platform::ToUtf16(leftUtf8);
    const auto right = wit::platform::ToUtf16(rightUtf8);
    const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT,
        LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
        left.c_str(), static_cast<int>(left.size()),
        right.c_str(), static_cast<int>(right.size()),
        nullptr, nullptr, 0);
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

void EnsureNaturalNoCaseCollation(sqlite3* db) {
    if (!db) return;
    sqlite3_create_collation_v2(db, "WIN_NATURAL_NOCASE", SQLITE_UTF8, nullptr,
        NaturalNoCaseCollation, nullptr);
}

const char* OrderExpressionFor(wit::core::FileSortColumn column, bool folders) {
    switch (column) {
    case wit::core::FileSortColumn::Type:
        return folders ? "c.entry_type"
            : "f.extension";
    case wit::core::FileSortColumn::Size:
        return folders ? "c.content_size" : "f.size";
    case wit::core::FileSortColumn::Path:
        return "p.path COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Modified:
        return folders ? "c.modified_at" : "f.modified_at";
    case wit::core::FileSortColumn::Name:
    default:
        return folders ? "c.name" : "f.name";
    }
}

std::string OrderByFor(wit::core::FileSort sort, bool folders) {
    if (sort.column == wit::core::FileSortColumn::Name) {
        const char* prefix = folders ? "c" : "f";
        return std::string{"ORDER BY "} + prefix + ".name" + (sort.ascending ? " ASC," : " DESC,") +
            prefix + ".id ASC ";
    }

    std::string order{"ORDER BY "};
    order += OrderExpressionFor(sort.column, folders);
    order += sort.ascending ? " ASC," : " DESC,";
    order += folders ? " c.name ASC,c.id ASC "
        : " f.name ASC,f.id ASC ";
    return order;
}

const char* RootOrderExpressionFor(int column) {
    switch (column) {
    case 1: return "disk_type COLLATE WIN_NATURAL_NOCASE";
    case 2: return "total_capacity";
    case 3: return "free_space";
    case 4: return "updated_at";
    case 5: return "CASE WHEN kind=0 THEN total_disks ELSE disk_number END";
    case 6: return "description COLLATE WIN_NATURAL_NOCASE";
    case 7: return "category COLLATE WIN_NATURAL_NOCASE";
    case 8: return "location COLLATE WIN_NATURAL_NOCASE";
    case 0:
    default:
        return "name COLLATE WIN_NATURAL_NOCASE";
    }
}

std::string RootOrderByFor(wit::core::BrowserRootSort sort) {
    std::string order{"ORDER BY kind ASC,"};
    order += RootOrderExpressionFor(sort.column);
    order += sort.ascending ? " ASC," : " DESC,";
    order += " name COLLATE WIN_NATURAL_NOCASE ASC,id ASC ";
    return order;
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
    const wit::core::BrowserLocation& location, int offset, int limit, wit::core::BrowserRootSort sort) {
    std::vector<wit::core::BrowserItem> items;
    EnsureNaturalNoCaseCollation(db_);
    if (location.isDiskGroup) {
        const auto sql = std::string(
            "WITH RECURSIVE group_tree(root_id,id) AS ("
            "SELECT id,id FROM disk_groups "
            "UNION ALL SELECT t.root_id,g.id FROM disk_groups g JOIN group_tree t ON g.parent_group_id=t.id) "
            "SELECT kind,id,disk_group_id,name,disk_number,source_path,total_capacity,free_space,updated_at,"
            "description,category,location,disk_type,total_disks,parent_group_id FROM ("
            "SELECT 0 AS kind,g.id AS id,NULL AS disk_group_id,g.name AS name,0 AS disk_number,'' AS source_path,"
            "COALESCE(SUM(d.total_capacity),0) AS total_capacity,COALESCE(SUM(d.free_space),0) AS free_space,"
            "g.updated_at AS updated_at,'' AS description,'' AS category,"
            "'' AS location,'Other' AS disk_type,COUNT(d.id) AS total_disks,g.parent_group_id AS parent_group_id "
            "FROM disk_groups g LEFT JOIN group_tree t ON t.root_id=g.id "
            "LEFT JOIN disks d ON d.disk_group_id=t.id WHERE g.parent_group_id=? "
            "GROUP BY g.id,g.name,g.updated_at,g.parent_group_id "
            "UNION ALL "
            "SELECT 1 AS kind,d.id,d.disk_group_id,d.disk_name,d.disk_number,d.source_path,d.total_capacity,"
            "d.free_space,d.updated_at,d.description,d.category,d.location,d.disk_type,0 AS total_disks,NULL AS parent_group_id "
            "FROM disks d WHERE d.disk_group_id=?) "
            ) + RootOrderByFor(sort) + "LIMIT ? OFFSET ?;";
        SQLiteStatement statement(db_, sql.c_str());
        statement.BindInt64(1, location.diskGroupId);
        statement.BindInt64(2, location.diskGroupId);
        statement.BindInt64(3, limit);
        statement.BindInt64(4, offset);
        while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
            wit::core::BrowserItem item;
            if (sqlite3_column_int(statement.Raw(), 0) == 0) {
                item.type = wit::core::BrowserItemType::DiskGroup;
                PopulateBrowserGroup(item.group, statement.Raw());
            } else {
                item.type = wit::core::BrowserItemType::Disk;
                PopulateBrowserDisk(item.disk, statement.Raw(), 1);
            }
            items.push_back(item);
        }
        return items;
    }
    const auto sql = std::string(
        "WITH RECURSIVE group_tree(root_id,id) AS ("
        "SELECT id,id FROM disk_groups "
        "UNION ALL SELECT t.root_id,g.id FROM disk_groups g JOIN group_tree t ON g.parent_group_id=t.id) "
        "SELECT kind,id,disk_group_id,name,disk_number,source_path,total_capacity,free_space,updated_at,"
        "description,category,location,disk_type,total_disks,parent_group_id FROM ("
        "SELECT 0 AS kind,g.id AS id,NULL AS disk_group_id,g.name AS name,0 AS disk_number,'' AS source_path,"
        "COALESCE(SUM(d.total_capacity),0) AS total_capacity,COALESCE(SUM(d.free_space),0) AS free_space,"
        "g.updated_at AS updated_at,'' AS description,'' AS category,"
        "'' AS location,'Other' AS disk_type,COUNT(d.id) AS total_disks,g.parent_group_id AS parent_group_id "
        "FROM disk_groups g LEFT JOIN group_tree t ON t.root_id=g.id "
        "LEFT JOIN disks d ON d.disk_group_id=t.id WHERE g.parent_group_id IS NULL "
        "GROUP BY g.id,g.name,g.updated_at,g.parent_group_id "
        "UNION ALL "
        "SELECT 1 AS kind,d.id,d.disk_group_id,d.disk_name,d.disk_number,d.source_path,d.total_capacity,"
        "d.free_space,d.updated_at,d.description,d.category,d.location,d.disk_type,0 AS total_disks,NULL AS parent_group_id "
        "FROM disks d WHERE d.disk_group_id IS NULL) "
        ) + RootOrderByFor(sort) + "LIMIT ? OFFSET ?;";
    SQLiteStatement statement(db_, sql.c_str());
    statement.BindInt64(1, limit);
    statement.BindInt64(2, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::BrowserItem item;
        if (sqlite3_column_int(statement.Raw(), 0) == 0) {
            item.type = wit::core::BrowserItemType::DiskGroup;
            PopulateBrowserGroup(item.group, statement.Raw());
        } else {
            item.type = wit::core::BrowserItemType::Disk;
            PopulateBrowserDisk(item.disk, statement.Raw(), 1);
        }
        items.push_back(item);
    }
    return items;
}

std::vector<wit::core::FileEntry> SqliteBrowserRepository::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit, wit::core::FileSort sort) {
    std::vector<wit::core::FileEntry> files;
    if (location.isRoot || location.isDiskGroup) return files;
    EnsureNaturalNoCaseCollation(db_);

    SQLiteStatement folderCountStatement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT COUNT(*) FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?;");
    folderCountStatement.BindInt64(1, location.sourceId);
    folderCountStatement.BindText(2, wit::platform::ToUtf8(location.path));
    folderCountStatement.BindInt64(3, location.sourceId);
    const int folderCount = sqlite3_step(folderCountStatement.Raw()) == SQLITE_ROW
        ? sqlite3_column_int(folderCountStatement.Raw(), 0) : 0;

    if (offset < folderCount && limit > 0) {
        const auto sql = std::string(
            "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
            "SELECT c.id,c.disk_id,(SELECT path FROM parent),c.name,'' AS extension,c.content_size,c.modified_at,c.attributes,1,c.entry_type,c.path "
            "FROM folders c "
            "WHERE c.disk_id=? AND c.parent_folder_id=(SELECT id FROM parent) ") + OrderByFor(sort, true) + "LIMIT ? OFFSET ?;";
        SQLiteStatement folderStatement(db_, sql.c_str());
        folderStatement.BindInt64(1, location.sourceId);
        folderStatement.BindText(2, wit::platform::ToUtf8(location.path));
        folderStatement.BindInt64(3, location.sourceId);
        folderStatement.BindInt64(4, limit);
        folderStatement.BindInt64(5, offset);
        while (sqlite3_step(folderStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, folderStatement.Raw());
            files.push_back(file);
        }
    }

    const int remaining = limit - static_cast<int>(files.size());
    if (remaining > 0) {
        const int fileOffset = (std::max)(0, offset - folderCount);
        const auto sql = std::string(
            "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
            "SELECT f.id,f.disk_id,(SELECT path FROM parent),f.name,f.extension,f.size,f.modified_at,f.attributes,0,'file',(SELECT path FROM parent) "
            "FROM files f "
            "WHERE f.disk_id=? AND f.folder_id=(SELECT id FROM parent) ") + OrderByFor(sort, false) + "LIMIT ? OFFSET ?;";
        SQLiteStatement fileStatement(db_, sql.c_str());
        fileStatement.BindInt64(1, location.sourceId);
        fileStatement.BindText(2, wit::platform::ToUtf8(location.path));
        fileStatement.BindInt64(3, location.sourceId);
        fileStatement.BindInt64(4, remaining);
        fileStatement.BindInt64(5, fileOffset);
        while (sqlite3_step(fileStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, fileStatement.Raw());
            files.push_back(file);
        }
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
        "SELECT c.id,c.disk_id,p.path,c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type,c.path "
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
