#pragma once
#include <Windows.h>
#include <CommCtrl.h>
#include <memory>
#include <string>
#include <vector>
#include "../core/BrowserLocation.h"
#include "../core/Catalog.h"
#include "../storage/Database.h"

namespace wit::ui {
class CatalogTreeView {
public:
    void Attach(HWND handle) { hwnd_ = handle; }
    void Clear();
    void Reload(const std::wstring& catalogLabel, const std::vector<wit::core::Catalog>& sources,
        wit::storage::Database* database);
    void Expand(HTREEITEM item);
    const wit::core::BrowserLocation* LocationFor(HTREEITEM item) const;
    bool SelectLocation(const wit::core::BrowserLocation& location);

private:
    struct Node {
        wit::core::BrowserLocation location;
        bool populated{};
    };

    HWND hwnd_{};
    HTREEITEM root_{};
    wit::storage::Database* db_{};
    std::vector<std::unique_ptr<Node>> nodes_;

    HTREEITEM InsertNode(HTREEITEM parent, const std::wstring& text,
        const wit::core::BrowserLocation& location, bool mayHaveChildren);
    HTREEITEM FindSource(std::int64_t sourceId) const;
};
}
