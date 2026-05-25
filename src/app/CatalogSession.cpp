#include "CatalogSession.h"
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <vector>

namespace wit::app {
namespace {

std::wstring NameForCatalogPath(const std::wstring& path) {
    if (path.size() == 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) return path;
    const auto end = path.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return path;
    const auto separator = path.find_last_of(L"\\/", end);
    return separator == std::wstring::npos ? path.substr(0, end + 1) :
        path.substr(separator + 1, end - separator);
}

std::wstring NormalizedCatalogPath(const std::wstring& path) {
    DWORD capacity = MAX_PATH;
    for (;;) {
        std::vector<wchar_t> buffer(capacity);
        const auto length = GetFullPathNameW(path.c_str(), capacity, buffer.data(), nullptr);
        if (length == 0) return path;
        if (length < capacity) return std::wstring(buffer.data(), length);
        capacity = length + 1;
    }
}

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
    const auto normalizedPath = NormalizedCatalogPath(path);
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
            return catalog.get();
        }
    }
    wit::storage::Database candidate;
    const bool opened = createNew ? candidate.CreateNew(normalizedPath) : candidate.OpenExisting(normalizedPath);
    if (!opened) return nullptr;

    auto catalog = std::make_unique<OpenCatalog>();
    catalog->id = nextCatalogId_++;
    if (catalog->id == 0) catalog->id = nextCatalogId_++;
    catalog->path = normalizedPath;
    catalog->label = NameForCatalogPath(normalizedPath);
    catalog->database = std::move(candidate);
    auto* result = catalog.get();
    catalogs_.push_back(std::move(catalog));
    activeCatalogId_ = result->id;
    if (persistPath) {
        settings_.lastCatalogPath = normalizedPath;
        wit::platform::RememberRecentCatalog(settings_, normalizedPath);
        settingsSaved = wit::platform::SaveAppSettings(settings_);
    }
    return result;
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
}

bool CatalogSession::SavePending(wit::core::CatalogId id) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog || !catalog->database.IsOpen() || catalog->path.empty()) return true;
    if (!catalog->database.IsEditable()) return false;
    if (!catalog->dirty || !catalog->pendingDatabase) return true;
    if (!catalog->database.ReplaceCatalogDataFrom(*catalog->pendingDatabase)) return false;
    catalog->pendingDatabase.reset();
    catalog->dirty = false;
    return true;
}

void CatalogSession::DiscardPending(wit::core::CatalogId id) {
    AssertOwnerThread();
    auto* catalog = Find(id);
    if (!catalog) return;
    catalog->pendingDatabase.reset();
    catalog->dirty = false;
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
