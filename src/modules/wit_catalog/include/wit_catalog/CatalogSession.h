#pragma once

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "wit_types/CatalogIdentity.h"
#include <wit_infra/AppSettings.h>
#include "wit_database/Database.h"

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
    [[nodiscard]] wit::storage::Database* WorkingDatabase() {
        return pendingDatabase ? pendingDatabase.get() : &database;
    }
};

class CatalogSession {
public:
    void LoadSettings();
    const wit::platform::AppSettings& Settings() const { return settings_; }
    [[nodiscard]] bool SaveSettings(const wit::platform::AppSettings& settings);

    [[nodiscard]] OpenCatalog* Open(const std::wstring& path, bool createNew, bool persistPath,
        bool& settingsSaved, bool& alreadyOpen);
    bool IsPathOpen(const std::wstring& path) const;
    [[nodiscard]] OpenCatalog* Find(wit::core::CatalogId id);
    [[nodiscard]] const OpenCatalog* Find(wit::core::CatalogId id) const;
    [[nodiscard]] OpenCatalog* ActiveCatalog();
    [[nodiscard]] const OpenCatalog* ActiveCatalog() const;
    [[nodiscard]] bool SetActive(wit::core::CatalogId id);
    bool HasCatalogs() const { return !catalogs_.empty(); }
    std::vector<OpenCatalog*> OpenCatalogs();

    void AcceptPending(wit::core::CatalogId id, std::unique_ptr<wit::storage::Database> pending);
    [[nodiscard]] bool SavePending(wit::core::CatalogId id);
    void DiscardPending(wit::core::CatalogId id);
    [[nodiscard]] bool Remove(wit::core::CatalogId id);

private:
    void AssertOwnerThread() const;

    std::vector<std::unique_ptr<OpenCatalog>> catalogs_;
    wit::core::CatalogId activeCatalogId_{};
    wit::core::CatalogId nextCatalogId_{1};
    wit::platform::AppSettings settings_;
    const std::thread::id ownerThreadId_{std::this_thread::get_id()};
};

}

