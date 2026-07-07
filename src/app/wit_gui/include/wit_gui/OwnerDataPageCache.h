#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace wit::ui {
template <typename T>
class OwnerDataPageCache {
public:
    struct Page {
        int start{};
        std::vector<T> items;
        unsigned long long lastUsed{};
    };

    OwnerDataPageCache(int pageSize, std::size_t maxPages) : pageSize_(pageSize), maxPages_(maxPages) {}

    [[nodiscard]] int PageSize() const { return pageSize_; }
    [[nodiscard]] int NormalizeStart(int row) const { return pageSize_ > 0 ? (row / pageSize_) * pageSize_ : 0; }

    void Clear() {
        cacheClock_ = 0;
        pages_.clear();
        pendingPageStart_ = -1;
    }

    [[nodiscard]] bool ContainsStart(int start) const {
        return std::ranges::find_if(pages_, [start](const Page& page) { return page.start == start; }) != pages_.end();
    }

    [[nodiscard]] std::optional<int> FirstMissingStartInRange(int firstRow, int lastRow, int total) const {
        if (total <= 0 || pageSize_ <= 0) return std::nullopt;
        firstRow = std::clamp(firstRow, 0, total - 1);
        lastRow = std::clamp(lastRow, firstRow, total - 1);
        const int firstPage = firstRow / pageSize_;
        const int lastPage = lastRow / pageSize_;
        for (int page = firstPage; page <= lastPage; ++page) {
            const int start = page * pageSize_;
            if (!ContainsStart(start)) return start;
        }
        return std::nullopt;
    }

    const T* EntryAt(int row, int total) {
        if (row < 0 || row >= total) return nullptr;
        const int pageStart = NormalizeStart(row);
        auto found = std::ranges::find_if(pages_, [pageStart](const Page& page) { return page.start == pageStart; });
        if (found == pages_.end()) return nullptr;
        found->lastUsed = ++cacheClock_;
        const int index = row - found->start;
        return index >= 0 && index < static_cast<int>(found->items.size()) ? &found->items[index] : nullptr;
    }

    std::pair<int, int> StorePage(Page page) {
        page.lastUsed = ++cacheClock_;
        const int first = page.start;
        const int last = first + static_cast<int>(page.items.size()) - 1;
        auto existing = std::ranges::find_if(pages_, [start = page.start](const Page& cached) {
            return cached.start == start;
        });
        if (existing != pages_.end()) {
            *existing = std::move(page);
        } else {
            pages_.push_back(std::move(page));
        }
        TrimOldest();
        return {first, last};
    }

    [[nodiscard]] const std::vector<Page>& Pages() const { return pages_; }

    [[nodiscard]] std::uint64_t BeginRequest() { return ++requestId_; }
    [[nodiscard]] bool IsCurrentRequest(std::uint64_t requestId) const { return requestId == requestId_; }
    void InvalidateRequests() { ++requestId_; }

    void SetPendingStart(int start) { pendingPageStart_ = start; }
    [[nodiscard]] bool HasPendingStart() const { return pendingPageStart_ >= 0; }
    [[nodiscard]] int TakePendingStart() { return std::exchange(pendingPageStart_, -1); }

private:
    void TrimOldest() {
        while (pages_.size() > maxPages_) {
            const auto oldest = std::ranges::min_element(pages_,
                [](const Page& left, const Page& right) { return left.lastUsed < right.lastUsed; });
            if (oldest == pages_.end()) break;
            pages_.erase(oldest);
        }
    }

    int pageSize_{};
    std::size_t maxPages_{};
    unsigned long long cacheClock_{};
    std::vector<Page> pages_;
    std::uint64_t requestId_{};
    int pendingPageStart_{-1};
};
}