#include "pf/pf_pager.h"
#include <cassert>
#include <unistd.h>

PfPager::PfPager() {
    for (size_t i = 0; i < NUM_CACHE_PAGES; i++) {
        _pages[i].buf = _cache + i * PAGE_SIZE;
        _pages[i].is_dirty = false;
        _free_pages.push_back(&_pages[i]);
    }
}

PfPager::~PfPager() {
    flush_all();
    assert(_free_pages.size() == NUM_CACHE_PAGES);
}

void PfPager::read_page(int fd, int page_no, uint8_t *buf, int num_bytes) {
    lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    ssize_t bytes_read = read(fd, buf, num_bytes);
    if (bytes_read != num_bytes) {
        throw UnixError();
    }
}

void PfPager::write_page(int fd, int page_no, const uint8_t *buf, int num_bytes) {
    lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    ssize_t bytes_write = write(fd, buf, num_bytes);
    if (bytes_write != num_bytes) {
        throw UnixError();
    }
}

Page *PfPager::create_page(int fd, int page_no) {
    Page *page = get_page<false>(fd, page_no);
    page->mark_dirty();
    return page;
}

Page *PfPager::fetch_page(int fd, int page_no) { return get_page<true>(fd, page_no); }

void PfPager::flush_file(int fd) {
    auto it_page = _busy_pages.begin();
    while (it_page != _busy_pages.end()) {
        auto prev_page = it_page;
        it_page++;
        if ((*prev_page)->id.fd == fd) {
            flush_page(*prev_page);
        }
    }
}

void PfPager::force_page(Page *page) {
    if (page->is_dirty) {
        write_page(page->id.fd, page->id.page_no, page->buf, PAGE_SIZE);
        page->is_dirty = false;
    }
}

template <bool EXISTS>
Page *PfPager::get_page(int fd, int page_no) {
    Page *page;
    PageId page_id(fd, page_no);
    auto map_it = _busy_map.find(page_id);
    if (map_it == _busy_map.end()) {
        // Page is not in memory (i.e. on disk). Allocate new cache page for it.
        if (_free_pages.empty()) {
            // Cache is full. Need to flush a page to disk.
            assert(!_busy_pages.empty());
            force_page(_busy_pages.back());
            _busy_map.erase(_busy_pages.back()->id);
            _busy_pages.splice(_busy_pages.begin(), _busy_pages, --_busy_pages.end());
        } else {
            // Cache is not full. Allocate from free pages.
            _busy_pages.splice(_busy_pages.begin(), _free_pages, _free_pages.begin());
        }
        _busy_map[page_id] = _busy_pages.begin();
        page = _busy_pages.front();
        page->id = page_id;
        page->is_dirty = false;
        if (EXISTS) {
            read_page(fd, page_no, page->buf, PAGE_SIZE);
        }
    } else {
        // Page is in memory
        page = *map_it->second;
        access(page);
    }
    return page;
}

void PfPager::access(Page *page) {
    assert(in_cache(page->id));
    _busy_pages.splice(_busy_pages.begin(), _busy_pages, _busy_map[page->id]);
}

void PfPager::flush_page(Page *page) {
    assert(in_cache(page->id));
    auto map_it = _busy_map.find(page->id);
    auto busy_it = map_it->second;
    force_page(page);
    _free_pages.splice(_free_pages.begin(), _busy_pages, busy_it);
    _busy_map.erase(map_it);
    assert(!in_cache(page->id));
}

void PfPager::flush_all() {
    for (Page *page : _busy_pages) {
        force_page(page);
    }
    _free_pages.insert(_free_pages.end(), _busy_pages.begin(), _busy_pages.end());
    _busy_pages.clear();
    _busy_map.clear();
}
