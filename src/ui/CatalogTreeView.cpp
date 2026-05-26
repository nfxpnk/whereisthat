#include "CatalogTreeView.h"
#include <algorithm>

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

void CatalogTreeView::Attach(HWND handle, DatabaseResolver databaseResolver) {
    hwnd_ = handle;
    databaseResolver_ = std::move(databaseResolver);
}

void CatalogTreeView::Clear() {
    if (hwnd_) TreeView_DeleteAllItems(hwnd_);
    roots_.clear();
    nodes_.clear();
}

HTREEITEM CatalogTreeView::InsertNode(HTREEITEM parent, wit::core::CatalogId catalogId,
    const std::wstring& text, const wit::core::BrowserLocation& location, bool catalogRoot,
    bool mayHaveChildren) {
    auto node = std::make_unique<Node>();
    node->target.catalogId = catalogId;
    node->target.location = location;
    node->catalogRoot = catalogRoot;
    node->populated = catalogRoot;
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

CatalogTreeView::Root* CatalogTreeView::FindRoot(wit::core::CatalogId id) {
    for (auto& root : roots_) if (root.id == id) return &root;
    return nullptr;
}

const CatalogTreeView::Root* CatalogTreeView::FindRoot(wit::core::CatalogId id) const {
    for (const auto& root : roots_) if (root.id == id) return &root;
    return nullptr;
}

void CatalogTreeView::PopulateRoot(Root& root, const std::wstring& label,
    const std::vector<wit::core::Catalog>& sources) {
    TVITEMW text{};
    text.mask = TVIF_TEXT | TVIF_CHILDREN;
    text.hItem = root.item;
    text.pszText = const_cast<LPWSTR>(label.c_str());
    text.cChildren = sources.empty() ? 0 : 1;
    TreeView_SetItem(hwnd_, &text);
    while (const auto child = TreeView_GetChild(hwnd_, root.item)) TreeView_DeleteItem(hwnd_, child);
    auto* database = databaseResolver_ ? databaseResolver_(root.id) : nullptr;
    for (const auto& source : sources) {
        wit::core::BrowserLocation location;
        location.isRoot = false;
        location.sourceId = source.id;
        location.sourceName = source.name;
        location.sourceRoot = source.rootPath;
        location.path = source.rootPath;
        InsertNode(root.item, root.id, source.name, location, false,
            database && database->HasChildFolders(location.sourceId, location.path));
    }
    TreeView_Expand(hwnd_, root.item, TVE_EXPAND);
}

void CatalogTreeView::AddCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
    const std::vector<wit::core::Catalog>& sources, bool select) {
    if (FindRoot(id)) {
        RefreshCatalog(id, catalogLabel, sources, select);
        return;
    }
    wit::core::BrowserLocation rootLocation;
    Root root{id, InsertNode(TVI_ROOT, id, catalogLabel, rootLocation, true, !sources.empty())};
    roots_.push_back(root);
    PopulateRoot(roots_.back(), catalogLabel, sources);
    if (select) TreeView_SelectItem(hwnd_, root.item);
}

void CatalogTreeView::RefreshCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
    const std::vector<wit::core::Catalog>& sources, bool select) {
    auto* root = FindRoot(id);
    if (!root) {
        AddCatalog(id, catalogLabel, sources, select);
        return;
    }
    PopulateRoot(*root, catalogLabel, sources);
    if (select) TreeView_SelectItem(hwnd_, root->item);
}

void CatalogTreeView::RemoveCatalog(wit::core::CatalogId id) {
    const auto position = std::find_if(roots_.begin(), roots_.end(),
        [id](const Root& root) { return root.id == id; });
    if (position == roots_.end()) return;
    TreeView_DeleteItem(hwnd_, position->item);
    roots_.erase(position);
}

const wit::core::BrowserTarget* CatalogTreeView::TargetFor(HTREEITEM item) const {
    if (!item) return nullptr;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM;
    treeItem.hItem = item;
    if (!TreeView_GetItem(hwnd_, &treeItem)) return nullptr;
    const auto* node = reinterpret_cast<const Node*>(treeItem.lParam);
    return node ? &node->target : nullptr;
}

wit::core::CatalogId CatalogTreeView::CatalogIdFor(HTREEITEM item) const {
    const auto* target = TargetFor(item);
    return target ? target->catalogId : 0;
}

bool CatalogTreeView::IsCatalogRoot(HTREEITEM item) const {
    if (!item) return false;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM;
    treeItem.hItem = item;
    if (!TreeView_GetItem(hwnd_, &treeItem)) return false;
    const auto* node = reinterpret_cast<const Node*>(treeItem.lParam);
    return node && node->catalogRoot;
}

void CatalogTreeView::Expand(HTREEITEM item) {
    if (!item || IsCatalogRoot(item)) return;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM | TVIF_CHILDREN;
    treeItem.hItem = item;
    if (!TreeView_GetItem(hwnd_, &treeItem)) return;
    auto* node = reinterpret_cast<Node*>(treeItem.lParam);
    if (!node || node->populated) return;
    auto* database = databaseResolver_ ? databaseResolver_(node->target.catalogId) : nullptr;
    if (!database) return;
    const auto& location = node->target.location;
    const auto folders = database->GetChildFolders(location.sourceId, location.path);
    for (const auto& folder : folders) {
        auto child = location;
        child.path = JoinPath(location.path, folder.name);
        //const auto label = folder.isArchive ? folder.name : folder.name;
        const auto label = folder.name;
        InsertNode(item, node->target.catalogId, label, child, false,
            database->HasChildFolders(child.sourceId, child.path));
    }
    node->populated = true;
    treeItem.mask = TVIF_CHILDREN;
    treeItem.cChildren = folders.empty() ? 0 : 1;
    TreeView_SetItem(hwnd_, &treeItem);
}

HTREEITEM CatalogTreeView::FindSource(wit::core::CatalogId catalogId, std::int64_t sourceId) const {
    const auto* root = FindRoot(catalogId);
    if (!root) return nullptr;
    for (auto item = TreeView_GetChild(hwnd_, root->item); item; item = TreeView_GetNextSibling(hwnd_, item)) {
        const auto* target = TargetFor(item);
        if (target && target->location.sourceId == sourceId) return item;
    }
    return nullptr;
}

bool CatalogTreeView::SelectLocation(const wit::core::BrowserTarget& target) {
    const auto* root = FindRoot(target.catalogId);
    if (!root) return false;
    const auto& location = target.location;
    if (location.isRoot) {
        TreeView_SelectItem(hwnd_, root->item);
        return true;
    }
    auto item = FindSource(target.catalogId, location.sourceId);
    if (!item) return false;
    auto current = TargetFor(item);
    while (current && !SameText(current->location.path, location.path)) {
        Expand(item);
        HTREEITEM matching{};
        for (auto child = TreeView_GetChild(hwnd_, item); child; child = TreeView_GetNextSibling(hwnd_, child)) {
            const auto* childTarget = TargetFor(child);
            if (childTarget && IncludesChildPath(location.path, childTarget->location.path)) {
                matching = child;
                break;
            }
        }
        if (!matching) return false;
        TreeView_Expand(hwnd_, item, TVE_EXPAND);
        item = matching;
        current = TargetFor(item);
    }
    if (!current || !SameText(current->location.path, location.path)) return false;
    TreeView_SelectItem(hwnd_, item);
    TreeView_EnsureVisible(hwnd_, item);
    return true;
}

bool CatalogTreeView::SelectCatalogRoot(wit::core::CatalogId id) {
    const auto* root = FindRoot(id);
    if (!root) return false;
    TreeView_SelectItem(hwnd_, root->item);
    return true;
}

bool CatalogTreeView::SelectFirstCatalogRoot() {
    if (roots_.empty()) return false;
    TreeView_SelectItem(hwnd_, roots_.front().item);
    return true;
}
}
