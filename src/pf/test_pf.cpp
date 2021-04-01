#undef NDEBUG

#define private public

#include "pf.h"

#undef private

#include <cassert>
#include <cstring>
#include <ctime>
#include <vector>
#include <unordered_map>
#include <string>

constexpr int MAX_FILES = 32;
constexpr int MAX_PAGES = 128;

std::unordered_map<int, Buffer> mock;   // fd -> buffer

Buffer mock_get_page(int fd, int page_no) {
    return &mock[fd][page_no * PAGE_SIZE];
}

void check_disk(int fd, int page_no) {
    static uint8_t buf[PAGE_SIZE];
    PfPager::read_page(fd, page_no, buf, PAGE_SIZE);
    Buffer mock_buf = mock_get_page(fd, page_no);
    assert(memcmp(buf, mock_buf, PAGE_SIZE) == 0);
}

void check_disk_all() {
    for (auto &file: mock) {
        int fd = file.first;
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_disk(fd, page_no);
        }
    }
}

void check_cache(int fd, int page_no) {
    Page *page = PfManager::pager.fetch_page(fd, page_no);
    Buffer mock_buf = mock_get_page(fd, page_no);
    assert(memcmp(page->buf, mock_buf, PAGE_SIZE) == 0);
}

void check_cache_all() {
    for (auto &file: mock) {
        int fd = file.first;
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_cache(fd, page_no);
        }
    }
}

void rand_buf(int size, OutputBuffer buf) {
    for (int i = 0; i < size; i++) {
        int rand_ch = rand() & 0xff;
        buf[i] = rand_ch;
    }
}

int rand_fd() {
    assert(mock.size() == MAX_FILES);
    int fd_idx = rand() % MAX_FILES;
    auto it = mock.begin();
    for (int i = 0; i < fd_idx; i++) {
        it++;
    }
    return it->first;
}

int main() {
    srand((unsigned) time(nullptr));

    // test files
    std::vector<std::string> filenames(MAX_FILES);
    std::unordered_map<int, std::string> fd2name;
    for (size_t i = 0; i < filenames.size(); i++) {
        auto &filename = filenames[i];
        filename = std::to_string(i) + ".txt";
        if (PfManager::is_file(filename)) {
            PfManager::destroy_file(filename);
        }
        // open without create
        try {
            PfManager::open_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {}

        PfManager::create_file(filename);
        assert(PfManager::is_file(filename));
        try {
            PfManager::create_file(filename);
            assert(false);
        } catch (const FileExistsError &e) {}

        // open file
        int fd = PfManager::open_file(filename);
        mock[fd] = new uint8_t[PAGE_SIZE * MAX_PAGES];
        fd2name[fd] = filename;
    }
    // init & test alloc_page
    int num_pages = 0;
    uint8_t init_buf[PAGE_SIZE];
    for (auto &fh: mock) {
        int fd = fh.first;
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            rand_buf(PAGE_SIZE, init_buf);
            Page *page = PfManager::pager.create_page(fd, page_no);
            memcpy(page->buf, init_buf, PAGE_SIZE);
            Buffer mock_buf = mock_get_page(fd, page_no);
            memcpy(mock_buf, init_buf, PAGE_SIZE);
            num_pages++;
        }
    }
    check_cache_all();
    // Test LRU cache
    assert(!PfManager::pager._busy_pages.empty());
    auto prev = PfManager::pager._busy_pages;
    for (int r = 0; r < 50; r++) {
        // Randomly access a page
        int idx = rand() % PfManager::pager._busy_pages.size();
        auto it = prev.begin();
        for (int i = 0; i < idx; i++) { it++; }
        Page *page = *it;
        prev.erase(it);
        prev.push_front(page);
        // Fetch this page with PF
        PfManager::pager.fetch_page(page->id.fd, page->id.page_no);
        // Assert this page is front of the list
        assert(prev == PfManager::pager._busy_pages);
        assert(PfManager::pager._hashmap[page->id] == PfManager::pager._busy_pages.begin());
    }
    // Flush and test disk
    PfManager::pager.flush_all();
    check_disk_all();
    // test get_page
    for (int r = 0; r < 10000; r++) {
        int fd = rand_fd();
        int page_no = rand() % MAX_PAGES;
        // get page
        Page *page = PfManager::pager.fetch_page(fd, page_no);
        Buffer mock_buf = mock_get_page(fd, page_no);
        assert(memcmp(page->buf, mock_buf, PAGE_SIZE) == 0);

        // modify
        rand_buf(PAGE_SIZE, init_buf);
        memcpy(page->buf, init_buf, PAGE_SIZE);
        memcpy(mock_buf, init_buf, PAGE_SIZE);
        PfPager::mark_dirty(page);

        // flush
        if (rand() % 10 == 0) {
            PfManager::pager.flush_page(page);
            check_disk(fd, page_no);
        }
        // flush entire file
        if (rand() % 100 == 0) {
            PfManager::pager.flush_file(fd);
        }
        // re-open file
        if (rand() % 100 == 0) {
            PfManager::close_file(fd);
            auto filename = fd2name[fd];
            Buffer buf = mock[fd];
            fd2name.erase(fd);
            mock.erase(fd);
            int new_fd = PfManager::open_file(filename);
            mock[new_fd] = buf;
            fd2name[new_fd] = filename;
        }
        // assert equal in cache
        check_cache(fd, page_no);
    }
    check_cache_all();
    PfManager::pager.flush_all();
    check_disk_all();
    // close and destroy files
    for (auto &entry: fd2name) {
        int fd = entry.first;
        auto &filename = entry.second;
        PfManager::close_file(fd);
        PfManager::destroy_file(filename);
        try {
            PfManager::destroy_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {}
    }
    return 0;
}
