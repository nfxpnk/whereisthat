#include <wit_catalog/CatalogSession.h>
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <optional>
#include <unordered_map>
#include <vector>
#include <wit_infra/Logging.h>
#include <wit_infra/PathHelpers.h>
#include <wit_infra/SaveProfiler.h>

namespace wit::app {
namespace {

bool SamePath(const std::wstring& left, const std::wstring& right) {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

}

void CatalogSession::AssertOwnerThread() const {
    assert(std::this_thread::get_id() == ownerThreadId_);
}

void CatalogSession::LoadSettings() {
    AssertOwnerThread();
    settings_ = wit::platform::LoadAppSettings();
}

bool CatalogSession::SaveSettings(const wit::platform::AppSettings& settings) {
    AssertOwnerThread();
    if (!wit::platform::SaveAppSettings(settings)) return false;
    settings_ = settings;
    return true;
}

OpenCatalog* CatalogSession::Open(const std::wstring& path, bool createNew, bool persistPath,
    bool& settingsSaved, bool& alreadyOpen) {
    AssertOwnerThread();
    const auto normalizedPath = std::filesystem::absolute(path).wstring();
    WIT_LOG_DEBUG(std::format(L"session open path='{}' normalized='{}' createNew={}",
        path, normalizedPath, createNew));
    alreadyOpen = false;
    settingsSaved = true;
    if (!createNew) {
        for (auto& catalog : catalogs_) {
            if (!SamePath(catalog->path, normalizedPath)) continue;
            activeCatalogId_ = catalog->id;
            alreadyOpen = true;
            if (persistPath) {
                settings_.lastCatalogPath = catalog->path;
                wit::platform::RememberRecentCatalog(settings_, catalog->path);
                settingsSaved = SaveOpenCatalogSettings();
            }
            WIT_LOG_INFO(std::format(L"session reused open catalog id={} path='{}'",
                catalog->id, catalog->path));
            return catalog.get();
        }
    }
    wit::storage::Database candidate;
    const bool opened = createNew ? candidate.CreateNew(normalizedPath, true) : candidate.OpenExisting(normalizedPath);
    if (!opened) {
        WIT_LOG_ERROR(std::format(L"session failed to open catalog path='{}' createNew={}",
            normalizedPath, createNew));
        return nullptr;
    }

    auto catalog = std::make_unique<OpenCatalog>();
    catalog->id = nextCatalogId_++;
    if (catalog->id == 0) catalog->id = nextCatalogId_++;
    catalog->path = normalizedPath;
    catalog->label = wit::platform::DisplayNameForPath(normalizedPath);
    catalog->database = std::move(candidate);
    auto* result = catalog.get();
    catalogs_.push_back(std::move(catalog));
    activeCatalogId_ = result->id;
    WIT_LOG_INFO(std::format(L"session opened catalog id={} path='{}' editable={}",
        result->id, result->path, result->IsEditable()));
    if (persistPath) {
        settings_.lastCatalogPath = normalizedPath;
        wit::platform::RememberRecentCatalog(settings_, normalizedPath);
        settingsSaved = SaveOpenCatalogSettings();
    }
    return result;
}

bool CatalogSession::SaveOpenCatalogSettings() {
    AssertOwnerThread();
    settings_.openCatalogPaths.clear();
    settings_.openCatalogPaths.reserve(catalogs_.size());
    for (const auto& catalog : catalogs_) {
        if (!catalog->path.empty()) settings_.openCatalogPaths.push_back(catalog->path);
    }
    settings_.lastActiveCatalog = LastActiveCatalogIndex();
    settings_.hasMultiCatalogSettings = true;
    const auto* active = ActiveCatalog();
    settings_.lastCatalogPath = active ? active->path : L"";
    return wit::platform::SaveAppSettings(settings_);
}

bool CatalogSession::IsPathOpen(const std::wstring& path) const {
    AssertOwnerThread();
    const auto normalizedPath = std::filesystem::absolute(path).wstring();
    for (const auto& catalog : catalogs_) {
        if (SamePath(catalog->path, normalizedPath)) return true;
    }
    return false;
}

OpenCatalog* CatalogSession::Find(wit::core::CatalogId id) {
    AssertOwnerThread();
    for (auto& catalog : catalogs_) {
        if (catalog->id == id) return catalog.get();
    }
    return nullptr;
}

const OpenCatalog* CatalogSession::Find(wit::core::CatalogId id) const {
    AssertOwnerThread();
    for (const auto& catalog : catalogs_) {
        if (catalog->id == id) return catalog.get();
    }
    return nullptr;
}

OpenCatalog* CatalogSession::ActiveCatalog() {
    return Find(activeCatalogId_);
}

const OpenCatalog* CatalogSession::ActiveCatalog() const {
    return Find(activeCatalogId_);
}

bool CatalogSession::SetActive(wit::core::CatalogId id) {
    AssertOwnerThread();
    if (!Find(id)) return false;
    activeCatalogId_ = id;
    return true;
}

std::vector<OpenCatalog*> CatalogSession::OpenCatalogs() {
    AssertOwnerThread();
    std::vector<OpenCatalog*> result;
    result.reserve(catalogs_.size());
    for (auto& catalog : catalogs_) result.push_back(catalog.get());
    return result;
}

bool CatalogSession::AcceptPending(wit::core::CatalogId id, std::unique_ptr<wit::storage::Database> pending) {
    const auto timer = wit::infra::CurrentSaveProfile()
        ? std::make_optional<wit::infra::ScopedSaveTimer>(
            wit::infra::CurrentSaveProfile()->timingsNs.acceptPending)
        : std::nullopt;
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog || !pending) return false;
    if (!catalog->pendingMetadataEdits.empty() && !ApplyPendingMetadataEdits(*catalog, *pending)) {
        WIT_LOG_ERROR(std::format(L"session failed to fold metadata edits into pending catalog id={}", id));
        return false;
    }
    catalog->pendingMetadataEdits.clear();
    catalog->pendingDatabase = std::move(pending);
    catalog->dirty = catalog->pendingDatabase != nullptr || !catalog->pendingMetadataEdits.empty();
    WIT_LOG_DEBUG(std::format(L"session accepted pending catalog id={} dirty={}", id, catalog->dirty));
    return true;
}

bool CatalogSession::RecordMoveDiskToGroup(wit::core::CatalogId id, std::int64_t diskId,
    std::int64_t diskGroupId) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return false;
    if (catalog->pendingDatabase) return catalog->pendingDatabase->MoveDiskToGroup(diskId, diskGroupId);
    if (!catalog->database.BeginTransaction()) return false;
    const bool valid = catalog->database.MoveDiskToGroup(diskId, diskGroupId);
    (void)catalog->database.Rollback();
    if (!valid) return false;
    auto found = std::find_if(catalog->pendingMetadataEdits.begin(), catalog->pendingMetadataEdits.end(),
        [diskId](const PendingMetadataEdit& edit) {
            return edit.kind == PendingMetadataEditKind::MoveDiskToGroup && edit.id == diskId;
        });
    if (found != catalog->pendingMetadataEdits.end()) {
        found->targetId = diskGroupId;
    } else {
        catalog->pendingMetadataEdits.push_back(
            {PendingMetadataEditKind::MoveDiskToGroup, diskId, diskGroupId});
    }
    catalog->dirty = true;
    WIT_LOG_DEBUG(std::format(L"session recorded metadata edit MoveDiskToGroup catalogId={} diskId={} targetGroupId={}",
        id, diskId, diskGroupId));
    return true;
}

bool CatalogSession::RecordMoveDiskGroupToGroup(wit::core::CatalogId id, std::int64_t diskGroupId,
    std::int64_t parentGroupId) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return false;
    if (catalog->pendingDatabase) return catalog->pendingDatabase->MoveDiskGroupToGroup(diskGroupId, parentGroupId);
    if (!CanMoveDiskGroupToGroup(*catalog, diskGroupId, parentGroupId)) return false;
    auto found = std::find_if(catalog->pendingMetadataEdits.begin(), catalog->pendingMetadataEdits.end(),
        [diskGroupId](const PendingMetadataEdit& edit) {
            return edit.kind == PendingMetadataEditKind::MoveDiskGroupToGroup && edit.id == diskGroupId;
        });
    if (found != catalog->pendingMetadataEdits.end()) {
        found->targetId = parentGroupId;
    } else {
        catalog->pendingMetadataEdits.push_back(
            {PendingMetadataEditKind::MoveDiskGroupToGroup, diskGroupId, parentGroupId});
    }
    catalog->dirty = true;
    WIT_LOG_DEBUG(std::format(
        L"session recorded metadata edit MoveDiskGroupToGroup catalogId={} groupId={} targetParentGroupId={}",
        id, diskGroupId, parentGroupId));
    return true;
}

bool CatalogSession::SavePending(wit::core::CatalogId id) {
    const auto timer = wit::infra::CurrentSaveProfile()
        ? std::make_optional<wit::infra::ScopedSaveTimer>(
            wit::infra::CurrentSaveProfile()->timingsNs.savePending)
        : std::nullopt;
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog || !catalog->database.IsOpen() || catalog->path.empty()) {
        WIT_LOG_WARN(std::format(L"session save pending ignored: catalog unavailable id={}", id));
        return true;
    }
    if (!catalog->database.IsEditable()) {
        WIT_LOG_WARN(std::format(L"session save pending rejected: catalog read-only id={} path='{}'",
            id, catalog->path));
        return false;
    }
    if (!catalog->dirty) {
        WIT_LOG_DEBUG(std::format(L"session save pending skipped: clean id={}", id));
        return true;
    }
    if (catalog->pendingDatabase) {
        WIT_LOG_INFO(std::format(L"session save full pending started id={} path='{}'", id, catalog->path));
        if (!catalog->database.SaveCatalogDataFrom(*catalog->pendingDatabase)) {
            WIT_LOG_ERROR(std::format(L"session save full pending failed id={} path='{}'", id, catalog->path));
            return false;
        }
        catalog->pendingDatabase.reset();
    }
    if (!catalog->pendingMetadataEdits.empty() && !SavePendingMetadataEdits(*catalog)) {
        WIT_LOG_ERROR(std::format(L"session save metadata pending failed id={} path='{}'", id, catalog->path));
        return false;
    }
    catalog->dirty = catalog->pendingDatabase != nullptr || !catalog->pendingMetadataEdits.empty();
    if (!catalog->dirty) {
        WIT_LOG_INFO(std::format(L"session save pending completed id={} path='{}'", id, catalog->path));
    }
    return !catalog->dirty;
}

bool CatalogSession::SavePendingMetadataEdits(OpenCatalog& catalog) {
    if (catalog.pendingMetadataEdits.empty()) return true;
    WIT_LOG_INFO(std::format(L"session save metadata pending started id={} edits={}",
        catalog.id, catalog.pendingMetadataEdits.size()));
    if (!catalog.database.BeginImmediateTransaction()) return false;
    bool success = ApplyPendingMetadataEdits(catalog, catalog.database);
    if (success) success = catalog.database.Commit();
    if (!success) {
        (void)catalog.database.Rollback();
        return false;
    }
    catalog.pendingMetadataEdits.clear();
    WIT_LOG_INFO(std::format(L"session save metadata pending completed id={}", catalog.id));
    return true;
}

bool CatalogSession::ApplyPendingMetadataEdits(OpenCatalog& catalog, wit::storage::Database& database) {
    for (const auto& edit : catalog.pendingMetadataEdits) {
        bool success{};
        switch (edit.kind) {
        case PendingMetadataEditKind::MoveDiskToGroup:
            success = database.MoveDiskToGroup(edit.id, edit.targetId);
            break;
        case PendingMetadataEditKind::MoveDiskGroupToGroup:
            success = database.MoveDiskGroupToGroup(edit.id, edit.targetId);
            break;
        }
        if (!success) return false;
    }
    return true;
}

bool CatalogSession::CanMoveDiskGroupToGroup(OpenCatalog& catalog, std::int64_t diskGroupId,
    std::int64_t parentGroupId) const {
    if (diskGroupId == 0 || diskGroupId == parentGroupId) return false;
    std::unordered_map<std::int64_t, std::int64_t> parents;
    for (const auto& group : catalog.database.GetDiskGroups()) {
        parents[group.id] = group.parentGroupId;
    }
    for (const auto& edit : catalog.pendingMetadataEdits) {
        if (edit.kind == PendingMetadataEditKind::MoveDiskGroupToGroup) parents[edit.id] = edit.targetId;
    }
    if (!parents.contains(diskGroupId)) return false;
    if (parentGroupId != 0 && !parents.contains(parentGroupId)) return false;
    for (auto current = parentGroupId; current != 0;) {
        if (current == diskGroupId) return false;
        const auto found = parents.find(current);
        current = found == parents.end() ? 0 : found->second;
    }
    return true;
}

void CatalogSession::DiscardPending(wit::core::CatalogId id) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return;
    catalog->pendingDatabase.reset();
    catalog->pendingMetadataEdits.clear();
    catalog->dirty = false;
    WIT_LOG_INFO(std::format(L"session discarded pending catalog id={}", id));
}

bool CatalogSession::Remove(wit::core::CatalogId id, bool* settingsSaved) {
    AssertOwnerThread();
    if (settingsSaved) *settingsSaved = true;
    const auto position = std::find_if(catalogs_.begin(), catalogs_.end(),
        [id](const auto& catalog) { return catalog->id == id; });
    if (position == catalogs_.end()) return false;
    catalogs_.erase(position);
    if (activeCatalogId_ == id) {
        activeCatalogId_ = catalogs_.empty() ? 0 : catalogs_.front()->id;
    }
    const bool saved = SaveOpenCatalogSettings();
    if (settingsSaved) *settingsSaved = saved;
    return true;
}

int CatalogSession::LastActiveCatalogIndex() const {
    if (activeCatalogId_ == 0) return 0;
    for (std::size_t index = 0; index < catalogs_.size(); ++index) {
        if (catalogs_[index]->id == activeCatalogId_) return static_cast<int>(index);
    }
    return 0;
}

}

