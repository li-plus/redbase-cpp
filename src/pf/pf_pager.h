#pragma once

#include "error.h"
#include "pf/pf_defs.h"
#include <list>
#include <unordered_map>

class PfPager {
    friend class PfTest;

  public:
    PfPager();

    ~PfPager();

    static void read_page(int fd, int page_no, uint8_t *buf, int num_bytes);

    static void write_page(int fd, int page_no, const uint8_t *buf, int num_bytes);

    Page *create_page(int fd, int page_no);

    Page *fetch_page(int fd, int page_no) { return get_page(fd, page_no, true); }

    static void mark_dirty(Page *page) { page->is_dirty = true; }

    void flush_file(int fd);

  private:
    bool page_in_cache(const PageId &page_id) const { return _hashmap.count(page_id) > 0; }

    static void force_page(Page *page);

    // Get the page from memory corresponding to the disk page.
    // If the page is not in memory, allocate a page and read the disk.
    Page *get_page(int fd, int page_no, bool exists);

    void access(Page *page);

    void flush_page(Page *page);

    void flush_all();

  private:
    uint8_t _cache[NUM_CACHE_PAGES * PAGE_SIZE];
    Page _pages[NUM_CACHE_PAGES];
    std::unordered_map<PageId, std::list<Page *>::iterator, PageIdHash> _hashmap;
    std::list<Page *> _busy_pages;
    std::list<Page *> _free_pages;
};
