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
    Page *page = get_page(fd, page_no, false);
    mark_dirty(page);
    return page;
}

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

Page *PfPager::get_page(int fd, int page_no, bool exists) {
    Page *page;
    PageId page_id = {.fd = fd, .page_no = page_no};
    auto it_page = _hashmap.find(page_id);
    if (it_page == _hashmap.end()) {
        // Page is not in memory (i.e. on disk). Allocate new cache page for it.
        if (_free_pages.empty()) {
            // Cache is full. Need to flush a page to disk.
            assert(!_busy_pages.empty());
            page = _busy_pages.back();
            force_page(page);
            _busy_pages.pop_back();
            _hashmap.erase(page->id);
        } else {
            // Allocate from free pages.
            page = *_free_pages.begin();
            _free_pages.pop_front();
        }
        page->id = {.fd = fd, .page_no = page_no};
        page->is_dirty = false;
        // Push to the list front
        _busy_pages.push_front(page);
        _hashmap[page->id] = _busy_pages.begin();
        if (exists) {
            read_page(fd, page_no, page->buf, PAGE_SIZE);
        }
    } else {
        // Page is in memory
        page = *it_page->second;
        access(page);
    }
    return page;
}

void PfPager::access(Page *page) {
    assert(page_in_cache(page->id));
    auto it = _hashmap[page->id];
    _busy_pages.erase(it);
    _busy_pages.push_front(page);
    _hashmap[page->id] = _busy_pages.begin();
}

void PfPager::flush_page(Page *page) {
    assert(page_in_cache(page->id));
    auto it = _hashmap[page->id];
    force_page(page);
    _busy_pages.erase(it);
    _free_pages.push_front(page);
    _hashmap.erase(page->id);
    assert(!page_in_cache(page->id));
}

void PfPager::flush_all() {
    for (Page *page : _busy_pages) {
        force_page(page);
    }
    _free_pages.insert(_free_pages.end(), _busy_pages.begin(), _busy_pages.end());
    _busy_pages.clear();
    _hashmap.clear();
}