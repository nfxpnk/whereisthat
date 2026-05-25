#pragma once

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "../core/CatalogIdentity.h"
#include "../platform/AppSettings.h"
#include "../storage/Database.h"

namespace wit::app {

struct OpenCatalog {
    wit::core::CatalogId id{};
    std::wstring path;
    std::wstring label;
    wit::storage::Database database;
    std::unique_ptr<wit::storage::Database> pendingDatabase;
    bool dirty{};

    bool IsOpen() const { return database.IsOpen(); }
    bool IsEditable() const { return database.IsEditable(); }
    bool HasPendingChanges() const { return dirty; }
    wit::storage::Database* WorkingDatabase() {
        return pendingDatabase ? pendingDatabase.get() : &database;
    }
};

class CatalogSession {
public:
    void LoadSettings();
    const wit::platform::AppSettings& Settings() const { return settings_; }
    bool SaveSettings(const wit::platform::AppSettings& settings);

    OpenCatalog* Open(const std::wstring& path, bool createNew, bool persistPath,
        bool& settingsSaved, bool& alreadyOpen);
    OpenCatalog* Find(wit::core::CatalogId id);
    const OpenCatalog* Find(wit::core::CatalogId id) const;
    OpenCatalog* ActiveCatalog();
    const OpenCatalog* ActiveCatalog() const;
    bool SetActive(wit::core::CatalogId id);
    bool HasCatalogs() const { return !catalogs_.empty(); }
    std::vector<OpenCatalog*> OpenCatalogs();

    void AcceptPending(wit::core::CatalogId id, std::unique_ptr<wit::storage::Database> pending);
    bool SavePending(wit::core::CatalogId id);
    void DiscardPending(wit::core::CatalogId id);
    bool Remove(wit::core::CatalogId id);

private:
    void AssertOwnerThread() const;

    std::vector<std::unique_ptr<OpenCatalog>> catalogs_;
    wit::core::CatalogId activeCatalogId_{};
    wit::core::CatalogId nextCatalogId_{1};
    wit::platform::AppSettings settings_;
    const std::thread::id ownerThreadId_{std::this_thread::get_id()};
};

}
