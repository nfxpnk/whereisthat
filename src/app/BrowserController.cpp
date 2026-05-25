#include "BrowserController.h"
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

void BrowserController::Attach(HWND tree, HWND files, HWND back, HWND forward, HWND address) {
    treeHandle_ = tree;
    filesHandle_ = files;
    backHandle_ = back;
    forwardHandle_ = forward;
    addressHandle_ = address;
    tree_.Attach(treeHandle_);
    files_.Attach(filesHandle_);
}

void BrowserController::Clear() {
    tree_.Clear();
    currentLocation_ = {};
    history_.clear();
    historyIndex_ = -1;
    files_.SetLocation(currentLocation_, nullptr);
    SetWindowTextW(addressHandle_, L"");
    UpdateNavigationControls();
}

std::wstring BrowserController::AddressFor(const wit::core::BrowserLocation& location,
    const std::wstring& catalogLabel) const {
    auto address = catalogLabel;
    if (location.isRoot) return address;
    address += L"\\" + location.sourceName;
    if (location.path.size() > location.sourceRoot.size()) {
        auto relative = location.path.substr(location.sourceRoot.size());
        if (!relative.empty() && relative.front() != L'\\' && relative.front() != L'/') {
            address += L"\\";
        }
        address += relative;
    }
    return address;
}

void BrowserController::UpdateNavigationControls() {
    EnableWindow(backHandle_, historyIndex_ > 0);
    EnableWindow(forwardHandle_, historyIndex_ >= 0 &&
        historyIndex_ + 1 < static_cast<int>(history_.size()));
}

void BrowserController::NavigateTo(const wit::core::BrowserLocation& location, bool addToHistory,
    wit::storage::Database* database, const std::wstring& catalogLabel) {
    if (!database || !database->IsOpen()) return;
    if (addToHistory) {
        if (historyIndex_ >= 0 && currentLocation_ == location) return;
        history_.resize(static_cast<std::size_t>(historyIndex_ + 1));
        history_.push_back(location);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }
    currentLocation_ = location;
    files_.SetLocation(currentLocation_, database);
    const auto address = AddressFor(currentLocation_, catalogLabel);
    SetWindowTextW(addressHandle_, address.c_str());
    selectingTree_ = true;
    tree_.SelectLocation(currentLocation_);
    selectingTree_ = false;
    UpdateNavigationControls();
}

void BrowserController::Reload(const std::wstring& catalogLabel, wit::storage::Database* database) {
    if (!database || !database->IsOpen()) {
        Clear();
        return;
    }
    selectingTree_ = true;
    tree_.Reload(catalogLabel, database->GetCatalogs(), database);
    selectingTree_ = false;
    history_.clear();
    historyIndex_ = -1;
    NavigateTo({}, true, database, catalogLabel);
}

void BrowserController::NavigateBack(wit::storage::Database* database, const std::wstring& catalogLabel) {
    if (historyIndex_ <= 0) return;
    --historyIndex_;
    NavigateTo(history_[historyIndex_], false, database, catalogLabel);
}

void BrowserController::NavigateForward(wit::storage::Database* database, const std::wstring& catalogLabel) {
    if (historyIndex_ < 0 || historyIndex_ + 1 >= static_cast<int>(history_.size())) return;
    ++historyIndex_;
    NavigateTo(history_[historyIndex_], false, database, catalogLabel);
}

LRESULT BrowserController::OnTreeSelectionChanged(LPNMHDR header, wit::storage::Database* database,
    const std::wstring& catalogLabel) {
    if (selectingTree_) return 0;
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(header);
    const auto* location = tree_.LocationFor(notification->itemNew.hItem);
    if (location) NavigateTo(*location, true, database, catalogLabel);
    return 0;
}

LRESULT BrowserController::OnTreeExpanding(LPNMHDR header) {
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(header);
    if ((notification->action & TVE_EXPAND) != 0) tree_.Expand(notification->itemNew.hItem);
    return 0;
}

LRESULT BrowserController::OnFileGetDispInfo(LPNMHDR header) {
    auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(header);
    if (displayInfo->item.mask & LVIF_TEXT) {
        auto text = files_.TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem);
        lstrcpynW(displayInfo->item.pszText, text.c_str(), displayInfo->item.cchTextMax);
    }
    return 0;
}

LRESULT BrowserController::OnFileActivate(LPNMHDR header, wit::storage::Database* database,
    const std::wstring& catalogLabel) {
    const auto* activation = reinterpret_cast<NMITEMACTIVATE*>(header);
    if (activation->iItem < 0) return 0;
    wit::core::BrowserLocation next;
    next.isRoot = false;
    if (currentLocation_.isRoot) {
        const auto* disk = files_.DiskAt(activation->iItem);
        if (!disk) return 0;
        next.sourceId = disk->id;
        next.sourceName = disk->diskName;
        next.sourceRoot = disk->sourcePath;
        next.path = disk->sourcePath;
    } else {
        const auto* entry = files_.EntryAt(activation->iItem);
        if (!entry || !entry->isDirectory) return 0;
        next = currentLocation_;
        next.path = JoinStoredPath(currentLocation_.path, entry->name);
    }
    NavigateTo(next, true, database, catalogLabel);
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
        auto text = entry->name;
        if (!entry->isDirectory) text += L" | " + CompactSize(entry->size);
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
                if (!entry->isDirectory) totalSize += entry->size;
            }
        }
    }
    return L"Selected item(s): " + std::to_wstring(selected) +
        L" (total: " + CompactSize(totalSize) + L")";
}

}
