#pragma once

#include "defs.h"
#include <cinttypes>
#include <cstdlib>

static constexpr int PAGE_SIZE = 4096;
static constexpr int NUM_CACHE_PAGES = 65536;

struct PageId {
    int fd;
    int page_no;

    PageId() = default;
    PageId(int fd_, int page_no_) : fd(fd_), page_no(page_no_) {}

    friend bool operator==(const PageId &x, const PageId &y) { return x.fd == y.fd && x.page_no == y.page_no; }
    friend bool operator!=(const PageId &x, const PageId &y) { return !(x == y); }

    friend std::ostream &operator<<(std::ostream &os, const PageId &self) {
        return os << "PageId(fd=" << self.fd << ", page_no=" << self.page_no << ")";
    }
};

namespace std {
template <>
struct hash<PageId> {
    size_t operator()(const PageId &pid) const noexcept { return (pid.fd << 16) | pid.page_no; }
};
} // namespace std

struct Page {
    PageId id;
    uint8_t *buf;
    bool is_dirty;

    void mark_dirty() { is_dirty = true; }
};
