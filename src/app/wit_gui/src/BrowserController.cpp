#include "wit_gui/BrowserController.h"
#include <algorithm>
#include <format>
#include <wit_infra/Logging.h>
#include <wit_infra/PathHelpers.h>
#include "wit_infra/StringUtils.h"
#include <wit_infra/Win32Helpers.h>

namespace wit::app {
namespace {

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
    if (location.isDiskGroup) return address + L"\\" + location.diskGroupName;
    if (location.diskGroupId != 0) address += L"\\" + location.diskGroupName;
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

void BrowserController::NavigateTo(const wit::core::BrowserTarget& target, bool addToHistory,
    bool syncTreeSelection) {
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
    files_.SetLocation(target.location, &database->BrowserRepository());
    const auto address = AddressFor(target);
    SetWindowTextW(addressHandle_, address.c_str());
    if (syncTreeSelection) {
        selectingTree_ = true;
        tree_.SelectLocation(target);
        selectingTree_ = false;
    }
    UpdateNavigationControls();
}

void BrowserController::AddCatalog(wit::core::CatalogId id, const std::wstring& label,
    wit::storage::Database* database, bool select) {
    if (!database || !database->IsOpen()) return;
    tree_.AddCatalog(id, label, database, select);
    if (select) NavigateTo({id, {}}, true);
}

void BrowserController::RefreshCatalog(wit::core::CatalogId id, const std::wstring& label,
    wit::storage::Database* database, bool select) {
    if (!database || !database->IsOpen()) return;
    if (hasTarget_ && currentTarget_.catalogId == id) {
        files_.SetLocation({}, nullptr);
    }
    tree_.RefreshCatalog(id, label, database, select);
    if (select || (hasTarget_ && currentTarget_.catalogId == id)) NavigateTo({id, {}}, true);
}

void BrowserController::MoveDiskToGroup(wit::core::CatalogId id, std::int64_t diskId,
    std::int64_t diskGroupId, wit::storage::Database* database) {
    if (!database || !database->IsOpen()) return;
    if (hasTarget_ && currentTarget_.catalogId == id) {
        files_.SetLocation({}, nullptr);
    }
    std::wstring diskGroupName;
    if (diskGroupId != 0) {
        for (const auto& group : database->GetDiskGroups()) {
            if (group.id == diskGroupId) {
                diskGroupName = group.name;
                break;
            }
        }
        if (diskGroupName.empty()) return;
    }
    if (!tree_.MoveDiskToGroup(id, diskId, diskGroupId, database)) {
        WIT_LOG_DEBUG(std::format(L"move disk tree update skipped catalogId={} diskId={} targetGroupId={}",
            id, diskId, diskGroupId));
        if (hasTarget_ && currentTarget_.catalogId == id) {
            files_.SetLocation(currentTarget_.location, &database->BrowserRepository());
        }
        return;
    }
    UpdateMovedDiskTargets(id, diskId, diskGroupId, diskGroupName);
    if (hasTarget_ && currentTarget_.catalogId == id) {
        files_.SetLocation(currentTarget_.location, &database->BrowserRepository());
        const auto address = AddressFor(currentTarget_);
        SetWindowTextW(addressHandle_, address.c_str());
    }
}

void BrowserController::RemoveCatalog(wit::core::CatalogId id) {
    tree_.RemoveCatalog(id);
    std::erase_if(history_, [id](const auto& target) { return target.catalogId == id; });
    historyIndex_ = history_.empty() ? -1 : (std::min)(historyIndex_, static_cast<int>(history_.size()) - 1);
    if (hasTarget_ && currentTarget_.catalogId == id) {
        currentTarget_ = {};
        hasTarget_ = false;
        files_.SetLocation({}, nullptr);
        SetWindowTextW(addressHandle_, L"");
    }
    UpdateNavigationControls();
}

void BrowserController::UpdateMovedDiskTargets(wit::core::CatalogId id, std::int64_t diskId,
    std::int64_t diskGroupId, const std::wstring& diskGroupName) {
    auto updateTarget = [&](wit::core::BrowserTarget& target) {
        auto& location = target.location;
        if (target.catalogId == id && location.sourceId == diskId) {
            location.diskGroupId = diskGroupId;
            location.diskGroupName = diskGroupName;
        }
    };
    if (hasTarget_) updateTarget(currentTarget_);
    for (auto& target : history_) updateTarget(target);
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

std::optional<wit::core::BrowserTarget> BrowserController::TargetForTreeItem(HTREEITEM item) const {
    const auto* target = tree_.TargetFor(item);
    if (!target) return std::nullopt;
    return *target;
}

std::optional<wit::core::BrowserTarget> BrowserController::SelectedTreeTarget() const {
    return TargetForTreeItem(TreeView_GetSelection(treeHandle_));
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

void BrowserController::RefreshDisplay() {
    if (filesHandle_) {
        files_.ResetCachedItems();
        RedrawWindow(filesHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

wit::core::CatalogId BrowserController::OnTreeSelectionChanged(LPNMHDR header) {
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(header);
    const auto* target = tree_.TargetFor(notification->itemNew.hItem);
    if (!target) return 0;
    if (!selectingTree_) NavigateTo(*target, true, false);
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
    if ((displayInfo->item.mask & LVIF_STATE) &&
        (displayInfo->item.stateMask & LVIS_SELECTED)) {
        if (files_.IsSelected(displayInfo->item.iItem)) {
            displayInfo->item.state |= LVIS_SELECTED;
        } else {
            displayInfo->item.state &= ~LVIS_SELECTED;
        }
    }
    if (displayInfo->item.mask & LVIF_TEXT) {
        files_.TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem,
            displayInfo->item.pszText, displayInfo->item.cchTextMax);
    }
    if ((displayInfo->item.mask & LVIF_STATE) == 0) displayInfo->item.mask |= LVIF_DI_SETITEM;
    return 0;
}

LRESULT BrowserController::OnFileCacheHint(LPNMHDR header) {
    const auto* hint = reinterpret_cast<NMLVCACHEHINT*>(header);
    files_.PreloadRange(hint->iFrom, hint->iTo);
    return 0;
}

LRESULT BrowserController::OnFileActivate(LPNMHDR header) {
    if (!hasTarget_) return 0;
    const auto* activation = reinterpret_cast<NMITEMACTIVATE*>(header);
    if (activation->iItem < 0) return 0;
    auto next = currentTarget_;
    next.location.isRoot = false;
    if (currentTarget_.location.isRoot || currentTarget_.location.isDiskGroup) {
        const auto* item = files_.BrowserItemAt(activation->iItem);
        if (!item) return 0;
        if (item->type == wit::core::BrowserItemType::DiskGroup) {
            next.location.isDiskGroup = true;
            next.location.diskGroupId = item->group.id;
            next.location.diskGroupName = item->group.name;
            NavigateTo(next, true);
            return 0;
        }
        const auto* disk = &item->disk;
        next.location.isDiskGroup = false;
        next.location.sourceId = disk->id;
        next.location.sourceName = disk->diskName;
        next.location.sourceRoot = disk->sourcePath;
        next.location.path = disk->sourcePath;
    } else {
        const auto* entry = files_.EntryAt(activation->iItem);
        if (!entry || !entry->isDirectory) return 0;
        next.location = currentTarget_.location;
        next.location.path = wit::platform::Join(currentTarget_.location.path, entry->name);
    }
    NavigateTo(next, true);
    return 0;
}

LRESULT BrowserController::OnFileClick(LPNMHDR header) {
    const auto* click = reinterpret_cast<NMITEMACTIVATE*>(header);
    if (!click || click->iItem < 0 || click->iItem >= files_.total) return 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0) return 0;
    files_.SelectOnly(click->iItem);
    RedrawWindow(filesHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    return 0;
}

bool BrowserController::FileItemStateChanged(LPNMHDR header) {
    const auto* changed = reinterpret_cast<NMLISTVIEW*>(header);
    if ((changed->uChanged & LVIF_STATE) && ((changed->uOldState ^ changed->uNewState) & LVIS_SELECTED) &&
        changed->iItem >= 0) {
        files_.SetSelectionRange(changed->iItem, changed->iItem, (changed->uNewState & LVIS_SELECTED) != 0);
    }
    return (changed->uChanged & LVIF_STATE) &&
        ((changed->uOldState ^ changed->uNewState) & (LVIS_FOCUSED | LVIS_SELECTED));
}

LRESULT BrowserController::OnFileOdStateChanged(LPNMHDR header) {
    const auto* changed = reinterpret_cast<NMLVODSTATECHANGE*>(header);
    if ((changed->uOldState ^ changed->uNewState) & LVIS_SELECTED) {
        files_.SetSelectionRange(changed->iFrom, changed->iTo, (changed->uNewState & LVIS_SELECTED) != 0);
    }
    return 0;
}

void BrowserController::SelectAll() {
    if (GetFocus() != filesHandle_ || files_.ShowsDisks()) return;
    const int focused = ListView_GetNextItem(filesHandle_, -1, LVNI_FOCUSED);
    if (focused < 0 || !files_.EntryAt(focused)) return;
    if (files_.SelectedCount() == files_.total) return;
    files_.SelectAllItems();
    RedrawWindow(filesHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void BrowserController::ClearFileSelection() {
    if (!filesHandle_ || files_.SelectedCount() == 0) return;
    files_.ClearSelection();
    RedrawWindow(filesHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void BrowserController::PrepareFileSingleSelection() {
    if (!filesHandle_ || files_.SelectedCount() == 0) return;
    files_.ClearSelection();
    InvalidateRect(filesHandle_, nullptr, FALSE);
}

std::wstring BrowserController::FocusedItemStatus() {
    const int index = filesHandle_ ? ListView_GetNextItem(filesHandle_, -1, LVNI_FOCUSED) : -1;
    if (const auto* item = index >= 0 ? files_.BrowserItemAt(index) : nullptr) {
        if (item->type == wit::core::BrowserItemType::DiskGroup) {
            return std::format(L"{} | Disks: {} | {}", item->group.name, item->group.totalDisks,
                CompactSize(item->group.totalCapacity));
        }
        auto text = item->disk.diskName + L" | " + CompactSize(item->disk.totalCapacity);
        const auto updatedAt = wit::platform::FormatUnixTimestamp(item->disk.updatedAt);
        if (!updatedAt.empty()) text += L" | " + updatedAt;
        return text;
    }
    if (const auto* entry = index >= 0 ? files_.EntryAt(index) : nullptr) {
        auto text = entry->name + L", " + CompactSize(entry->size);
        const auto modifiedAt = wit::platform::FormatUnixDate(entry->modifiedAt);
        if (!modifiedAt.empty()) text += L", " + modifiedAt;
        return text;
    }
    return {};
}

std::wstring BrowserController::SelectionSummaryStatus() {
    const int selected = files_.SelectedCount();
    if (selected <= 0) return std::format(L"Selected item(s): {} (total: {})", 0, CompactSize(0));

    std::uint64_t totalSize{};
    constexpr int kMaxMeasuredSelectionRows = 10000;
    if (selected > kMaxMeasuredSelectionRows) return std::format(L"Selected item(s): {}", selected);
    int measured{};
    for (const auto index : files_.SelectedRows(kMaxMeasuredSelectionRows)) {
        if (const auto* item = files_.BrowserItemAt(index)) {
            ++measured;
            totalSize += item->type == wit::core::BrowserItemType::Disk
                ? item->disk.totalCapacity : item->group.totalCapacity;
        } else if (const auto* entry = files_.EntryAt(index)) {
            ++measured;
            totalSize += entry->size;
        }
    }
    if (measured == selected) {
        return std::format(L"Selected item(s): {} (total: {})", selected, CompactSize(totalSize));
    }
    return std::format(L"Selected item(s): {}", selected);
}

}

