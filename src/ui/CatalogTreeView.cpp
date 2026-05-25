#include "CatalogTreeView.h"
#include <CommCtrl.h>

namespace wit::ui {
namespace {
std::wstring JoinPath(const std::wstring& parent, const std::wstring& name) {
    if (parent.empty()) return name;
    const auto last = parent.back();
    return last == L'\\' || last == L'/' ? parent + name : parent + L"\\" + name;
}

bool SameText(const std::wstring& left, const std::wstring& right) {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool IncludesChildPath(const std::wstring& target, const std::wstring& child) {
    if (SameText(target, child)) return true;
    if (target.size() <= child.size()) return false;
    if (!SameText(target.substr(0, child.size()), child)) return false;
    return target[child.size()] == L'\\' || target[child.size()] == L'/';
}
}

void CatalogTreeView::Clear() {
    if (hwnd_) TreeView_DeleteAllItems(hwnd_);
    root_ = nullptr;
    db_ = nullptr;
    nodes_.clear();
}

HTREEITEM CatalogTreeView::InsertNode(HTREEITEM parent, const std::wstring& text,
    const wit::core::BrowserLocation& location, bool mayHaveChildren) {
    auto node = std::make_unique<Node>();
    node->location = location;
    node->populated = location.isRoot;
    auto* nodePointer = node.get();
    nodes_.push_back(std::move(node));

    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
    insert.item.pszText = const_cast<LPWSTR>(text.c_str());
    insert.item.lParam = reinterpret_cast<LPARAM>(nodePointer);
    insert.item.cChildren = mayHaveChildren ? 1 : 0;
    return TreeView_InsertItem(hwnd_, &insert);
}

void CatalogTreeView::Reload(const std::wstring& catalogLabel,
    const std::vector<wit::core::Catalog>& sources, wit::storage::Database* database) {
    Clear();
    db_ = database;
    wit::core::BrowserLocation rootLocation;
    root_ = InsertNode(TVI_ROOT, catalogLabel, rootLocation, !sources.empty());
    for (const auto& source : sources) {
        wit::core::BrowserLocation location;
        location.isRoot = false;
        location.sourceId = source.id;
        location.sourceName = source.name;
        location.sourceRoot = source.rootPath;
        location.path = source.rootPath;
        InsertNode(root_, source.name, location,
            db_ && db_->HasChildFolders(location.sourceId, location.path));
    }
    TreeView_Expand(hwnd_, root_, TVE_EXPAND);
    TreeView_SelectItem(hwnd_, root_);
}

const wit::core::BrowserLocation* CatalogTreeView::LocationFor(HTREEITEM item) const {
    if (!item) return nullptr;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM;
    treeItem.hItem = item;
    if (!TreeView_GetItem(hwnd_, &treeItem)) return nullptr;
    const auto* node = reinterpret_cast<const Node*>(treeItem.lParam);
    return node ? &node->location : nullptr;
}

void CatalogTreeView::Expand(HTREEITEM item) {
    if (!db_ || !item || item == root_) return;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM | TVIF_CHILDREN;
    treeItem.hItem = item;
    if (!TreeView_GetItem(hwnd_, &treeItem)) return;
    auto* node = reinterpret_cast<Node*>(treeItem.lParam);
    if (!node || node->populated) return;

    const auto folders = db_->GetChildFolders(node->location.sourceId, node->location.path);
    for (const auto& folder : folders) {
        auto child = node->location;
        child.path = JoinPath(node->location.path, folder.name);
        InsertNode(item, folder.name, child,
            db_->HasChildFolders(child.sourceId, child.path));
    }
    node->populated = true;
    treeItem.mask = TVIF_CHILDREN;
    treeItem.cChildren = folders.empty() ? 0 : 1;
    TreeView_SetItem(hwnd_, &treeItem);
}

HTREEITEM CatalogTreeView::FindSource(std::int64_t sourceId) const {
    for (auto item = TreeView_GetChild(hwnd_, root_); item; item = TreeView_GetNextSibling(hwnd_, item)) {
        const auto* location = LocationFor(item);
        if (location && location->sourceId == sourceId) return item;
    }
    return nullptr;
}

bool CatalogTreeView::SelectLocation(const wit::core::BrowserLocation& location) {
    if (location.isRoot) {
        TreeView_SelectItem(hwnd_, root_);
        return root_ != nullptr;
    }
    auto item = FindSource(location.sourceId);
    if (!item) return false;
    auto current = LocationFor(item);
    while (current && !SameText(current->path, location.path)) {
        Expand(item);
        HTREEITEM matching{};
        for (auto child = TreeView_GetChild(hwnd_, item); child; child = TreeView_GetNextSibling(hwnd_, child)) {
            const auto* childLocation = LocationFor(child);
            if (childLocation && IncludesChildPath(location.path, childLocation->path)) {
                matching = child;
                break;
            }
        }
        if (!matching) return false;
        TreeView_Expand(hwnd_, item, TVE_EXPAND);
        item = matching;
        current = LocationFor(item);
    }
    if (!current || !SameText(current->path, location.path)) return false;
    TreeView_SelectItem(hwnd_, item);
    TreeView_EnsureVisible(hwnd_, item);
    return true;
}
}
