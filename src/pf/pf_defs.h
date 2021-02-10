#pragma once

#include "defs.h"
#include <cstdlib>
#include <cinttypes>

constexpr int PAGE_SIZE = 4096;
constexpr int NUM_CACHE_PAGES = 65536;

struct PageId {
    int fd;
    int page_no;

    friend bool operator==(const PageId &x, const PageId &y) {
        return x.fd == y.fd && x.page_no == y.page_no;
    }
};

struct PageIdHash {
    size_t operator()(const PageId &x) const { return (x.fd << 16) | x.page_no; }
};

struct Page {
    PageId id;
    Buffer buf;
    bool is_dirty;
};
