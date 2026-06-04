#include "wit_gui/TreeViewPane.h"
#include "wit_gui/BrowserItemIcons.h"
#include <wit_infra/PathHelpers.h>
#include <algorithm>
#include <functional>

namespace wit::ui {
namespace {
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
    bool mayHaveChildren, int image, HTREEITEM insertAfter) {
    auto node = std::make_unique<Node>();
    node->target.catalogId = catalogId;
    node->target.location = location;
    node->catalogRoot = catalogRoot;
    node->populated = catalogRoot;
    auto* nodePointer = node.get();
    nodes_.push_back(std::move(node));

    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = insertAfter;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    if (catalogRoot) {
        insert.item.mask |= TVIF_STATE;
        insert.item.stateMask = TVIS_BOLD;
        insert.item.state = TVIS_BOLD;
    }
    insert.item.pszText = const_cast<LPWSTR>(text.c_str());
    insert.item.lParam = reinterpret_cast<LPARAM>(nodePointer);
    insert.item.cChildren = mayHaveChildren ? 1 : 0;
    insert.item.iImage = image;
    insert.item.iSelectedImage = image;
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

void CatalogTreeView::PopulateRoot(Root& root, const std::wstring& label, wit::storage::Database* database) {
    TVITEMW text{};
    text.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_STATE;
    text.stateMask = TVIS_BOLD;
    text.state = TVIS_BOLD;
    text.hItem = root.item;
    text.pszText = const_cast<LPWSTR>(label.c_str());
    const auto itemCount = database ? database->GetBrowserRootItemCount({}) : 0;
    text.cChildren = itemCount == 0 ? 0 : 1;
    TreeView_SetItem(hwnd_, &text);
    while (const auto child = TreeView_GetChild(hwnd_, root.item)) TreeView_DeleteItem(hwnd_, child);
    if (!database) return;

    std::function<void(HTREEITEM, const wit::core::BrowserLocation&)> populateContainer =
        [&](HTREEITEM parentItem, const wit::core::BrowserLocation& parentLocation) {
            const auto count = database->GetBrowserRootItemCount(parentLocation);
            const auto items = database->GetBrowserRootItemsPage(parentLocation, 0, count);
            for (const auto& item : items) {
                if (item.type == wit::core::BrowserItemType::DiskGroup) {
                    wit::core::BrowserLocation groupLocation;
                    groupLocation.isRoot = false;
                    groupLocation.isDiskGroup = true;
                    groupLocation.diskGroupId = item.group.id;
                    groupLocation.diskGroupName = item.group.name;
                    const auto childCount = database->GetBrowserRootItemCount(groupLocation);
                    const auto groupItem = InsertNode(parentItem, root.id, item.group.name, groupLocation, false,
                        childCount != 0, BrowserFolderImage);
                    populateContainer(groupItem, groupLocation);
                    continue;
                }
                const auto& source = item.disk;
                wit::core::BrowserLocation location;
                location.isRoot = false;
                location.diskGroupId = parentLocation.isDiskGroup ? parentLocation.diskGroupId : 0;
                location.diskGroupName = parentLocation.isDiskGroup ? parentLocation.diskGroupName : L"";
                location.sourceId = source.id;
                location.sourceName = source.diskName;
                location.sourceRoot = source.sourcePath;
                location.path = source.sourcePath;
                InsertNode(parentItem, root.id, source.diskName, location, false,
                    database->HasChildFolders(location.sourceId, location.path), BrowserDriveImage);
            }
        };

    populateContainer(root.item, {});
    TreeView_Expand(hwnd_, root.item, TVE_EXPAND);
}

void CatalogTreeView::AddCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
    wit::storage::Database* database, bool select) {
    if (FindRoot(id)) {
        RefreshCatalog(id, catalogLabel, database, select);
        return;
    }
    wit::core::BrowserLocation rootLocation;
    Root root{id, InsertNode(TVI_ROOT, id, catalogLabel, rootLocation, true,
        database && database->GetBrowserRootItemCount(rootLocation) != 0, BrowserDatabaseImage)};
    roots_.push_back(root);
    PopulateRoot(roots_.back(), catalogLabel, database);
    if (select) TreeView_SelectItem(hwnd_, root.item);
}

void CatalogTreeView::RefreshCatalog(wit::core::CatalogId id, const std::wstring& catalogLabel,
    wit::storage::Database* database, bool select) {
    auto* root = FindRoot(id);
    if (!root) {
        AddCatalog(id, catalogLabel, database, select);
        return;
    }
    PopulateRoot(*root, catalogLabel, database);
    if (select) TreeView_SelectItem(hwnd_, root->item);
}

bool CatalogTreeView::MoveDiskToGroup(wit::core::CatalogId id, std::int64_t diskId,
    std::int64_t diskGroupId, wit::storage::Database* database) {
    if (!hwnd_ || !database) return false;
    const auto* root = FindRoot(id);
    if (!root) return false;
    const auto sourceItem = FindSource(id, diskId);
    const auto groupItem = diskGroupId == 0 ? root->item : FindDiskGroup(id, diskGroupId);
    if (!sourceItem || !groupItem) return false;

    std::wstring groupName;
    std::int64_t groupDiskCount{};
    if (diskGroupId != 0) {
        for (const auto& group : database->GetDiskGroups()) {
            if (group.id == diskGroupId) {
                groupName = group.name;
                groupDiskCount = database->GetBrowserRootItemCount({false, true, diskGroupId, group.name});
                break;
            }
        }
        if (groupName.empty()) return false;
    } else {
        groupDiskCount = database->GetBrowserRootItemCount({});
    }

    const auto sourceTarget = TargetFor(sourceItem);
    if (!sourceTarget) return false;
    const auto oldParent = TreeView_GetParent(hwnd_, sourceItem);
    const bool wasSelected = TreeView_GetSelection(hwnd_) == sourceItem;
    const bool oldParentExpanded = oldParent &&
        (TreeView_GetItemState(hwnd_, oldParent, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
    const bool newParentExpanded = (TreeView_GetItemState(hwnd_, groupItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
    const auto insertAfter = FindSortedInsertAfter(groupItem, sourceTarget->location.sourceName, sourceItem);

    SendMessageW(hwnd_, WM_SETREDRAW, FALSE, 0);
    const auto movedItem = CloneDisplayedSubtree(sourceItem, groupItem, insertAfter, diskGroupId, groupName);
    if (!movedItem) {
        SendMessageW(hwnd_, WM_SETREDRAW, TRUE, 0);
        return false;
    }
    TreeView_DeleteItem(hwnd_, sourceItem);
    SetMayHaveChildren(oldParent, TreeView_GetChild(hwnd_, oldParent) != nullptr);
    SetMayHaveChildren(groupItem, groupDiskCount > 0);
    if (oldParentExpanded) TreeView_Expand(hwnd_, oldParent, TVE_EXPAND);
    if (newParentExpanded) TreeView_Expand(hwnd_, groupItem, TVE_EXPAND);
    if (wasSelected) {
        TreeView_SelectItem(hwnd_, movedItem);
        TreeView_EnsureVisible(hwnd_, movedItem);
    }
    SendMessageW(hwnd_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd_, nullptr, FALSE);
    OutputDebugStringW(L"WhereIsThat: MoveDiskToGroup updated one TreeView disk node without RefreshCatalog.\n");
    return movedItem != nullptr;
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
    if (node->target.location.isDiskGroup) {
        node->populated = true;
        return;
    }
    auto* database = databaseResolver_ ? databaseResolver_(node->target.catalogId) : nullptr;
    if (!database) return;
    const auto& location = node->target.location;
    const auto folders = database->GetChildFolders(location.sourceId, location.path);
    for (const auto& folder : folders) {
        auto child = location;
        child.path = wit::platform::Join(location.path, folder.name);
        InsertNode(item, node->target.catalogId, folder.name, child, false,
            database->HasChildFolders(child.sourceId, child.path),
            folder.isArchive ? BrowserArchiveImage : BrowserFolderImage);
    }
    node->populated = true;
    treeItem.mask = TVIF_CHILDREN;
    treeItem.cChildren = folders.empty() ? 0 : 1;
    TreeView_SetItem(hwnd_, &treeItem);
}

HTREEITEM CatalogTreeView::FindSource(wit::core::CatalogId catalogId, std::int64_t sourceId) const {
    const auto* root = FindRoot(catalogId);
    if (!root) return nullptr;
    std::function<HTREEITEM(HTREEITEM)> findInChildren = [&](HTREEITEM parent) -> HTREEITEM {
        for (auto item = TreeView_GetChild(hwnd_, parent); item; item = TreeView_GetNextSibling(hwnd_, item)) {
            const auto* target = TargetFor(item);
            if (target && target->location.sourceId == sourceId) return item;
            if (const auto found = findInChildren(item)) return found;
        }
        return nullptr;
    };
    return findInChildren(root->item);
}

HTREEITEM CatalogTreeView::FindDiskGroup(wit::core::CatalogId catalogId, std::int64_t diskGroupId) const {
    const auto* root = FindRoot(catalogId);
    if (!root) return nullptr;
    std::function<HTREEITEM(HTREEITEM)> findInChildren = [&](HTREEITEM parent) -> HTREEITEM {
        for (auto item = TreeView_GetChild(hwnd_, parent); item; item = TreeView_GetNextSibling(hwnd_, item)) {
            const auto* target = TargetFor(item);
            if (target && target->location.isDiskGroup && target->location.diskGroupId == diskGroupId) {
                return item;
            }
            if (const auto found = findInChildren(item)) return found;
        }
        return nullptr;
    };
    return findInChildren(root->item);
}

HTREEITEM CatalogTreeView::FindSortedInsertAfter(HTREEITEM parent, const std::wstring& text,
    HTREEITEM excluding) const {
    HTREEITEM previous = TVI_FIRST;
    for (auto item = TreeView_GetChild(hwnd_, parent); item; item = TreeView_GetNextSibling(hwnd_, item)) {
        if (item == excluding) continue;
        wchar_t buffer[512]{};
        TVITEMW treeItem{};
        treeItem.mask = TVIF_TEXT;
        treeItem.hItem = item;
        treeItem.pszText = buffer;
        treeItem.cchTextMax = ARRAYSIZE(buffer);
        if (TreeView_GetItem(hwnd_, &treeItem) &&
            CompareStringOrdinal(text.c_str(), -1, buffer, -1, TRUE) == CSTR_LESS_THAN) {
            return previous;
        }
        previous = item;
    }
    return previous == TVI_FIRST ? TVI_FIRST : previous;
}

HTREEITEM CatalogTreeView::CloneDisplayedSubtree(HTREEITEM source, HTREEITEM parent, HTREEITEM insertAfter,
    std::int64_t diskGroupId, const std::wstring& diskGroupName) {
    wchar_t text[512]{};
    TVITEMW sourceItem{};
    sourceItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_STATE;
    sourceItem.stateMask = TVIS_EXPANDED;
    sourceItem.hItem = source;
    sourceItem.pszText = text;
    sourceItem.cchTextMax = ARRAYSIZE(text);
    if (!TreeView_GetItem(hwnd_, &sourceItem)) return nullptr;
    const auto* sourceNode = reinterpret_cast<const Node*>(sourceItem.lParam);
    if (!sourceNode) return nullptr;

    auto location = sourceNode->target.location;
    location.diskGroupId = diskGroupId;
    location.diskGroupName = diskGroupName;
    const auto cloned = InsertNode(parent, sourceNode->target.catalogId, text, location,
        sourceNode->catalogRoot, sourceItem.cChildren != 0, sourceItem.iImage, insertAfter);
    if (!cloned) return nullptr;

    TVITEMW clonedItem{};
    clonedItem.mask = TVIF_PARAM;
    clonedItem.hItem = cloned;
    if (TreeView_GetItem(hwnd_, &clonedItem)) {
        auto* clonedNode = reinterpret_cast<Node*>(clonedItem.lParam);
        if (clonedNode) {
            clonedNode->target = sourceNode->target;
            clonedNode->target.location = location;
            clonedNode->catalogRoot = sourceNode->catalogRoot;
            clonedNode->populated = sourceNode->populated;
        }
    }

    HTREEITEM lastChild = TVI_FIRST;
    for (auto child = TreeView_GetChild(hwnd_, source); child; child = TreeView_GetNextSibling(hwnd_, child)) {
        const auto clonedChild = CloneDisplayedSubtree(child, cloned, lastChild, diskGroupId, diskGroupName);
        if (clonedChild) lastChild = clonedChild;
    }
    if ((sourceItem.state & TVIS_EXPANDED) != 0) TreeView_Expand(hwnd_, cloned, TVE_EXPAND);
    return cloned;
}

void CatalogTreeView::SetMayHaveChildren(HTREEITEM item, bool mayHaveChildren) {
    if (!item) return;
    TVITEMW treeItem{};
    treeItem.mask = TVIF_CHILDREN;
    treeItem.hItem = item;
    treeItem.cChildren = mayHaveChildren ? 1 : 0;
    TreeView_SetItem(hwnd_, &treeItem);
}

bool CatalogTreeView::SelectLocation(const wit::core::BrowserTarget& target) {
    const auto* root = FindRoot(target.catalogId);
    if (!root) return false;
    const auto& location = target.location;
    if (location.isRoot) {
        TreeView_SelectItem(hwnd_, root->item);
        return true;
    }
    if (location.isDiskGroup) {
        std::function<HTREEITEM(HTREEITEM)> findInChildren = [&](HTREEITEM parent) -> HTREEITEM {
            for (auto item = TreeView_GetChild(hwnd_, parent); item; item = TreeView_GetNextSibling(hwnd_, item)) {
                const auto* itemTarget = TargetFor(item);
                if (itemTarget && itemTarget->location.isDiskGroup &&
                    itemTarget->location.diskGroupId == location.diskGroupId) {
                    return item;
                }
                if (const auto found = findInChildren(item)) return found;
            }
            return nullptr;
        };
        if (const auto item = findInChildren(root->item)) {
            TreeView_SelectItem(hwnd_, item);
            TreeView_EnsureVisible(hwnd_, item);
            return true;
        }
        return false;
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

