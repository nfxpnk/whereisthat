#include "CatalogSession.h"
#include <cassert>

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

bool CatalogSession::Activate(const std::wstring& path, bool createNew, bool persistPath, bool& settingsSaved) {
    AssertOwnerThread();
    wit::storage::Database candidate;
    const bool opened = createNew ? candidate.CreateNew(path) : candidate.OpenExisting(path);
    if (!opened) return false;

    database_ = std::move(candidate);
    pendingDatabase_.reset();
    dirty_ = false;
    activePath_ = path;
    settingsSaved = true;
    if (persistPath) {
        settings_.lastCatalogPath = path;
        wit::platform::RememberRecentCatalog(settings_, path);
        settingsSaved = wit::platform::SaveAppSettings(settings_);
    }
    return true;
}

std::wstring CatalogSession::CatalogLabel() const {
    return activePath_.empty() ? L"" : NameForCatalogPath(activePath_);
}

wit::storage::Database* CatalogSession::WorkingDatabase() {
    AssertOwnerThread();
    return pendingDatabase_ ? pendingDatabase_.get() : &database_;
}

void CatalogSession::AcceptPending(std::unique_ptr<wit::storage::Database> pending) {
    AssertOwnerThread();
    pendingDatabase_ = std::move(pending);
    dirty_ = pendingDatabase_ != nullptr;
}

bool CatalogSession::SavePending() {
    AssertOwnerThread();
    if (!database_.IsOpen() || activePath_.empty()) return true;
    if (!database_.IsEditable()) return false;
    if (!dirty_ || !pendingDatabase_) return true;
    if (!database_.ReplaceCatalogDataFrom(*pendingDatabase_)) return false;
    pendingDatabase_.reset();
    dirty_ = false;
    return true;
}

void CatalogSession::DiscardPending() {
    AssertOwnerThread();
    pendingDatabase_.reset();
    dirty_ = false;
}

}
