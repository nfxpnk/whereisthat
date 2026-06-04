#include <wit_catalog/CatalogSession.h>
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <vector>
#include <wit_infra/Logging.h>
#include <wit_infra/PathHelpers.h>

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
                settingsSaved = wit::platform::SaveAppSettings(settings_);
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
        settingsSaved = wit::platform::SaveAppSettings(settings_);
    }
    return result;
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

void CatalogSession::AcceptPending(wit::core::CatalogId id, std::unique_ptr<wit::storage::Database> pending) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return;
    catalog->pendingDatabase = std::move(pending);
    catalog->dirty = catalog->pendingDatabase != nullptr;
    WIT_LOG_DEBUG(std::format(L"session accepted pending catalog id={} dirty={}", id, catalog->dirty));
}

bool CatalogSession::SavePending(wit::core::CatalogId id) {
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
    if (!catalog->dirty || !catalog->pendingDatabase) {
        WIT_LOG_DEBUG(std::format(L"session save pending skipped: clean id={}", id));
        return true;
    }
    WIT_LOG_INFO(std::format(L"session save pending started id={} path='{}'", id, catalog->path));
    if (!catalog->database.SaveCatalogDataFrom(*catalog->pendingDatabase)) {
        WIT_LOG_ERROR(std::format(L"session save pending failed id={} path='{}'", id, catalog->path));
        return false;
    }
    catalog->pendingDatabase.reset();
    catalog->dirty = false;
    WIT_LOG_INFO(std::format(L"session save pending completed id={} path='{}'", id, catalog->path));
    return true;
}

void CatalogSession::DiscardPending(wit::core::CatalogId id) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return;
    catalog->pendingDatabase.reset();
    catalog->dirty = false;
    WIT_LOG_INFO(std::format(L"session discarded pending catalog id={}", id));
}

bool CatalogSession::Remove(wit::core::CatalogId id) {
    AssertOwnerThread();
    const auto position = std::find_if(catalogs_.begin(), catalogs_.end(),
        [id](const auto& catalog) { return catalog->id == id; });
    if (position == catalogs_.end()) return false;
    catalogs_.erase(position);
    if (activeCatalogId_ == id) {
        activeCatalogId_ = catalogs_.empty() ? 0 : catalogs_.front()->id;
    }
    return true;
}

}

