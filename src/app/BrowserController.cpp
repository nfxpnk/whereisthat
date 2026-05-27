#include "BrowserController.h"
#include <algorithm>
#include "../core/SizeFormat.h"
#include "../platform/Win32Helpers.h"

namespace wit::app {
namespace {

std::wstring JoinStoredPath(const std::wstring& parent, const std::wstring& name) {
    if (parent.empty()) return name;
    const auto last = parent.back();
    return last == L'\\' || last == L'/' ? parent + name : parent + L"\\" + name;
}

std::wstring CompactSize(std::uint64_t bytes) {
    auto result = wit::core::FormatSize(bytes);
    const auto decimal = result.find(L'.');
    const auto space = result.find(L' ');
    if (decimal != std::wstring::npos && space != std::wstring::npos) {
        auto end = space;
        while (end > decimal + 1 && result[end - 1] == L'0') result.erase(--end, 1);
        if (end == decimal + 1) result.erase(decimal, 1);
    }
    return result;
}

}

void BrowserController::Attach(HWND tree, HWND files, HWND back, HWND forward, HWND address,
    DatabaseResolver databaseResolver, LabelResolver labelResolver) {
    treeHandle_ = tree;
    filesHandle_ = files;
    backHandle_ = back;
    forwardHandle_ = forward;
    addressHandle_ = address;
    databaseResolver_ = std::move(databaseResolver);
    labelResolver_ = std::move(labelResolver);
    tree_.Attach(treeHandle_, databaseResolver_);
    files_.Attach(filesHandle_);
}

void BrowserController::Clear() {
    tree_.Clear();
    currentTarget_ = {};
    hasTarget_ = false;
    history_.clear();
    historyIndex_ = -1;
    files_.SetLocation({}, nullptr);
    SetWindowTextW(addressHandle_, L"");
    UpdateNavigationControls();
}

std::wstring BrowserController::AddressFor(const wit::core::BrowserTarget& target) const {
    const auto& location = target.location;
    auto address = labelResolver_ ? labelResolver_(target.catalogId) : L"";
    if (location.isRoot) return address;
    address += L"\\" + location.sourceName;
    if (location.path.size() > location.sourceRoot.size()) {
        auto relative = location.path.substr(location.sourceRoot.size());
        if (!relative.empty() && relative.front() != L'\\' && relative.front() != L'/') address += L"\\";
        address += relative;
    }
    return address;
}

void BrowserController::UpdateNavigationControls() {
    EnableWindow(backHandle_, historyIndex_ > 0);
    EnableWindow(forwardHandle_, historyIndex_ >= 0 &&
        historyIndex_ + 1 < static_cast<int>(history_.size()));
}

void BrowserController::NavigateTo(const wit::core::BrowserTarget& target, bool addToHistory) {
    auto* database = databaseResolver_ ? databaseResolver_(target.catalogId) : nullptr;
    if (!database || !database->IsOpen()) return;
    if (addToHistory) {
        if (historyIndex_ >= 0 && hasTarget_ && currentTarget_ == target) return;
        history_.resize(static_cast<std::size_t>(historyIndex_ + 1));
        history_.push_back(target);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }
    currentTarget_ = target;
    hasTarget_ = true;
    files_.SetLocation(target.location, database);
    const auto address = AddressFor(target);
    SetWindowTextW(addressHandle_, address.c_str());
    selectingTree_ = true;
    tree_.SelectLocation(target);
    selectingTree_ = false;
    UpdateNavigationControls();
}

void BrowserController::AddCatalog(wit::core::CatalogId id, const std::wstring& label,
    wit::storage::Database* database, bool select) {
    if (!database || !database->IsOpen()) return;
    tree_.AddCatalog(id, label, database->GetCatalogs(), select);
    if (select) NavigateTo({id, {}}, true);
}

void BrowserController::RefreshCatalog(wit::core::CatalogId id, const std::wstring& label,
    wit::storage::Database* database, bool select) {
    if (!database || !database->IsOpen()) return;
    tree_.RefreshCatalog(id, label, database->GetCatalogs(), select);
    if (select || (hasTarget_ && currentTarget_.catalogId == id)) NavigateTo({id, {}}, true);
}

void BrowserController::RemoveCatalog(wit::core::CatalogId id) {
    tree_.RemoveCatalog(id);
    history_.erase(std::remove_if(history_.begin(), history_.end(),
        [id](const auto& target) { return target.catalogId == id; }), history_.end());
    historyIndex_ = history_.empty() ? -1 : (std::min)(historyIndex_, static_cast<int>(history_.size()) - 1);
    if (hasTarget_ && currentTarget_.catalogId == id) {
        currentTarget_ = {};
        hasTarget_ = false;
        files_.SetLocation({}, nullptr);
        SetWindowTextW(addressHandle_, L"");
    }
    UpdateNavigationControls();
}

bool BrowserController::SelectCatalogRoot(wit::core::CatalogId id) {
    return tree_.SelectCatalogRoot(id);
}

bool BrowserController::SelectFirstCatalogRoot() {
    return tree_.SelectFirstCatalogRoot();
}

wit::core::CatalogId BrowserController::SelectedCatalogId() const {
    return tree_.CatalogIdFor(TreeView_GetSelection(treeHandle_));
}

void BrowserController::NavigateBack() {
    if (historyIndex_ <= 0) return;
    --historyIndex_;
    NavigateTo(history_[historyIndex_], false);
}

void BrowserController::NavigateForward() {
    if (historyIndex_ < 0 || historyIndex_ + 1 >= static_cast<int>(history_.size())) return;
    ++historyIndex_;
    NavigateTo(history_[historyIndex_], false);
}

wit::core::CatalogId BrowserController::OnTreeSelectionChanged(LPNMHDR header) {
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(header);
    const auto* target = tree_.TargetFor(notification->itemNew.hItem);
    if (!target) return 0;
    if (!selectingTree_) NavigateTo(*target, true);
    return target->catalogId;
}

LRESULT BrowserController::OnTreeExpanding(LPNMHDR header) {
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(header);
    if ((notification->action & TVE_EXPAND) != 0) tree_.Expand(notification->itemNew.hItem);
    return 0;
}

LRESULT BrowserController::OnFileGetDispInfo(LPNMHDR header) {
    auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(header);
    if (displayInfo->item.mask & LVIF_IMAGE) {
        displayInfo->item.iImage = files_.ImageFor(displayInfo->item.iItem);
    }
    if (displayInfo->item.mask & LVIF_TEXT) {
        auto text = files_.TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem);
        lstrcpynW(displayInfo->item.pszText, text.c_str(), displayInfo->item.cchTextMax);
    }
    return 0;
}

LRESULT BrowserController::OnFileCustomDraw(LPNMHDR header) const {
    return files_.OnCustomDraw(header);
}

LRESULT BrowserController::OnFileActivate(LPNMHDR header) {
    if (!hasTarget_) return 0;
    const auto* activation = reinterpret_cast<NMITEMACTIVATE*>(header);
    if (activation->iItem < 0) return 0;
    auto next = currentTarget_;
    next.location.isRoot = false;
    if (currentTarget_.location.isRoot) {
        const auto* disk = files_.DiskAt(activation->iItem);
        if (!disk) return 0;
        next.location.sourceId = disk->id;
        next.location.sourceName = disk->diskName;
        next.location.sourceRoot = disk->sourcePath;
        next.location.path = disk->sourcePath;
    } else {
        const auto* entry = files_.EntryAt(activation->iItem);
        if (!entry || !entry->isDirectory) return 0;
        next.location = currentTarget_.location;
        next.location.path = JoinStoredPath(currentTarget_.location.path, entry->name);
    }
    NavigateTo(next, true);
    return 0;
}

bool BrowserController::FileItemStateChanged(LPNMHDR header) const {
    const auto* changed = reinterpret_cast<NMLISTVIEW*>(header);
    return (changed->uChanged & LVIF_STATE) &&
        ((changed->uOldState ^ changed->uNewState) & (LVIS_FOCUSED | LVIS_SELECTED));
}

void BrowserController::SelectAll() {
    if (GetFocus() != filesHandle_ || files_.ShowsDisks()) return;
    const int focused = ListView_GetNextItem(filesHandle_, -1, LVNI_FOCUSED);
    if (focused < 0 || !files_.EntryAt(focused)) return;
    if (ListView_GetSelectedCount(filesHandle_) == files_.total) return;
    ListView_SetItemState(filesHandle_, -1, LVIS_SELECTED, LVIS_SELECTED);
}

std::wstring BrowserController::FocusedItemStatus() {
    const int index = filesHandle_ ? ListView_GetNextItem(filesHandle_, -1, LVNI_FOCUSED) : -1;
    if (const auto* disk = index >= 0 ? files_.DiskAt(index) : nullptr) {
        auto text = disk->diskName + L" | " + CompactSize(disk->totalCapacity);
        const auto updatedAt = wit::platform::FormatUnixTimestamp(disk->updatedAt);
        if (!updatedAt.empty()) text += L" | " + updatedAt;
        return text;
    }
    if (const auto* entry = index >= 0 ? files_.EntryAt(index) : nullptr) {
        auto text = entry->name + L" | " + CompactSize(entry->size);
        if (!entry->modifiedAt.empty()) text += L" | " + entry->modifiedAt;
        return text;
    }
    return {};
}

std::wstring BrowserController::SelectionSummaryStatus() {
    std::uint64_t totalSize{};
    int selected{};
    if (filesHandle_) {
        for (int index = ListView_GetNextItem(filesHandle_, -1, LVNI_SELECTED); index >= 0;
            index = ListView_GetNextItem(filesHandle_, index, LVNI_SELECTED)) {
            if (const auto* disk = files_.DiskAt(index)) {
                ++selected;
                totalSize += disk->totalCapacity;
            } else if (const auto* entry = files_.EntryAt(index)) {
                ++selected;
                totalSize += entry->size;
            }
        }
    }
    return L"Selected item(s): " + std::to_wstring(selected) +
        L" (total: " + CompactSize(totalSize) + L")";
}

}
