#pragma once

#include <memory>
#include <string>
#include <thread>
#include "../platform/AppSettings.h"
#include "../storage/Database.h"

namespace wit::app {

class CatalogSession {
public:
    void LoadSettings();
    const wit::platform::AppSettings& Settings() const { return settings_; }
    bool SaveSettings(const wit::platform::AppSettings& settings);

    bool Activate(const std::wstring& path, bool createNew, bool persistPath, bool& settingsSaved);
    bool IsOpen() const { return database_.IsOpen(); }
    bool IsEditable() const { return database_.IsEditable(); }
    bool HasPendingChanges() const { return dirty_; }
    const std::wstring& ActivePath() const { return activePath_; }
    std::wstring CatalogLabel() const;

    wit::storage::Database* WorkingDatabase();
    void AcceptPending(std::unique_ptr<wit::storage::Database> pending);
    bool SavePending();
    void DiscardPending();

private:
    void AssertOwnerThread() const;

    std::wstring activePath_;
    wit::storage::Database database_;
    // Pending scan state is adopted and accessed only by the UI thread.
    std::unique_ptr<wit::storage::Database> pendingDatabase_;
    bool dirty_{};
    wit::platform::AppSettings settings_;
    const std::thread::id ownerThreadId_{std::this_thread::get_id()};
};

}
