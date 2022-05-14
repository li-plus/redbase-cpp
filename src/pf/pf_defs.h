#pragma once

#include "defs.h"
#include <cinttypes>
#include <cstdlib>

constexpr int PAGE_SIZE = 4096;
constexpr int NUM_CACHE_PAGES = 65536;

struct PageId {
    int fd;
    int page_no;

    friend bool operator==(const PageId &x, const PageId &y) { return x.fd == y.fd && x.page_no == y.page_no; }
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
