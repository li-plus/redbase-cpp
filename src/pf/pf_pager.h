#pragma once

#include "error.h"
#include "pf/pf_defs.h"
#include <list>
#include <unordered_map>

class PfPager {
  public:
    PfPager();
    ~PfPager();

    PfPager &operator=(const PfPager &other) = delete;

    static void read_page(int fd, int page_no, uint8_t *buf, int num_bytes);
    static void write_page(int fd, int page_no, const uint8_t *buf, int num_bytes);

    Page *create_page(int fd, int page_no);

    Page *fetch_page(int fd, int page_no);

    void flush_file(int fd);
    void flush_page(Page *page);
    void flush_all();

    bool in_cache(const PageId &page_id) const { return _busy_map.find(page_id) != _busy_map.end(); }

    const std::list<Page *> &busy_list() const { return _busy_pages; }
    const std::unordered_map<PageId, std::list<Page *>::iterator> &busy_map() const { return _busy_map; }
    const std::list<Page *> &free_list() const { return _free_pages; }

  private:
    static void force_page(Page *page);

    // Get the page from memory corresponding to the disk page.
    // If the page is not in memory, allocate a page and read the disk.
    template <bool EXISTS>
    Page *get_page(int fd, int page_no);

    void access(Page *page);

  private:
    uint8_t _cache[NUM_CACHE_PAGES * PAGE_SIZE];
    Page _pages[NUM_CACHE_PAGES];
    std::unordered_map<PageId, std::list<Page *>::iterator> _busy_map;
    std::list<Page *> _busy_pages;
    std::list<Page *> _free_pages;
};
