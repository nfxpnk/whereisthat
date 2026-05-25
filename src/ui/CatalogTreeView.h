#pragma once
#include <Windows.h>
#include <CommCtrl.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../core/CatalogIdentity.h"
#include "../core/Catalog.h"
#include "../storage/Database.h"

namespace wit::ui {
class CatalogTreeView {
public:
    using DatabaseResolver = std::function<wit::storage::Database*(wit::core::CatalogId)>;

    void Attach(HWND handle, DatabaseResolver databaseResolver);
    void Clear();
    void AddCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
        const std::vector<wit::core::Catalog>& sources, bool select);
    void RefreshCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
        const std::vector<wit::core::Catalog>& sources, bool select);
    void RemoveCatalog(wit::core::CatalogId id);
    void Expand(HTREEITEM item);
    const wit::core::BrowserTarget* TargetFor(HTREEITEM item) const;
    wit::core::CatalogId CatalogIdFor(HTREEITEM item) const;
    bool IsCatalogRoot(HTREEITEM item) const;
    bool SelectLocation(const wit::core::BrowserTarget& target);
    bool SelectCatalogRoot(wit::core::CatalogId id);
    bool SelectFirstCatalogRoot();

private:
    struct Node {
        wit::core::BrowserTarget target;
        bool catalogRoot{};
        bool populated{};
    };
    struct Root {
        wit::core::CatalogId id{};
        HTREEITEM item{};
    };

    HWND hwnd_{};
    DatabaseResolver databaseResolver_;
    std::vector<Root> roots_;
    std::vector<std::unique_ptr<Node>> nodes_;

    HTREEITEM InsertNode(HTREEITEM parent, wit::core::CatalogId catalogId, const std::wstring& text,
        const wit::core::BrowserLocation& location, bool catalogRoot, bool mayHaveChildren);
    Root* FindRoot(wit::core::CatalogId id);
    const Root* FindRoot(wit::core::CatalogId id) const;
    HTREEITEM FindSource(wit::core::CatalogId catalogId, std::int64_t sourceId) const;
    void PopulateRoot(Root& root, const std::wstring& label, const std::vector<wit::core::Catalog>& sources);
};
}
