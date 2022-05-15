#include "pf/pf.h"
#include <gtest/gtest.h>

TEST(PfManagerTest, basic) {
    std::string path1 = "a.txt";
    if (PfManager::is_file(path1)) {
        PfManager::destroy_file(path1);
    }
    std::string path2 = "b.txt";
    if (PfManager::is_file(path2)) {
        PfManager::destroy_file(path2);
    }

    // create file
    PfManager::create_file(path1);
    EXPECT_THROW(PfManager::create_file(path1), FileExistsError);

    // is file
    EXPECT_TRUE(PfManager::is_file(path1));
    EXPECT_FALSE(PfManager::is_file(path2));

    // open file
    int fd = PfManager::open_file(path1);
    EXPECT_THROW(PfManager::open_file(path2), FileNotFoundError);
    EXPECT_THROW(PfManager::open_file(path1), FileNotClosedError);

    // close file
    EXPECT_THROW(PfManager::close_file(fd + 1), FileNotOpenError);
    PfManager::close_file(fd);
    EXPECT_THROW(PfManager::close_file(fd), FileNotOpenError);

    // destroy file
    EXPECT_THROW(PfManager::destroy_file(path2), FileNotFoundError);
    fd = PfManager::open_file(path1);
    EXPECT_THROW(PfManager::destroy_file(path1), FileNotClosedError);
    PfManager::close_file(fd);
    PfManager::destroy_file(path1);
    EXPECT_THROW(PfManager::destroy_file(path1), FileNotFoundError);
}

static void check_pages(const std::list<PageId> &busy_page_ids) {
    EXPECT_EQ(PfManager::pager.free_list().size(), NUM_CACHE_PAGES - busy_page_ids.size());
    EXPECT_EQ(PfManager::pager.busy_list().size(), busy_page_ids.size());
    auto busy_it = PfManager::pager.busy_list().begin();
    for (const auto &pid : busy_page_ids) {
        EXPECT_EQ(pid, (*busy_it)->id);
        EXPECT_TRUE(PfManager::pager.in_cache(pid));
        EXPECT_EQ(PfManager::pager.busy_map().at(pid), busy_it);
        busy_it++;
    }
}

TEST(PfPagerTest, lru) {
    srand((unsigned)time(nullptr));

    std::vector<std::string> paths{"0.txt", "1.txt", "2.txt", "3.txt"};
    std::vector<int> fds;
    for (const auto &path : paths) {
        if (PfManager::is_file(path)) {
            PfManager::destroy_file(path);
        }
        PfManager::create_file(path);
        int fd = PfManager::open_file(path);
        fds.emplace_back(fd);
    }

    // create pages
    std::list<PageId> busy_page_ids;
    for (int i = 0; i < NUM_CACHE_PAGES; i++) {
        if (rand() % 100 == 0) {
            check_pages(busy_page_ids);
        }
        for (int fd : fds) {
            PfManager::pager.create_page(fd, i);
            busy_page_ids.push_front(PageId(fd, i));
            if (busy_page_ids.size() > NUM_CACHE_PAGES) {
                busy_page_ids.pop_back();
            }
        }
    }
    check_pages(busy_page_ids);

    // access pages
    auto busy_it = busy_page_ids.begin();
    while (busy_it != busy_page_ids.end()) {
        if (rand() % 100 == 0) {
            check_pages(busy_page_ids);
        }
        PfManager::pager.fetch_page(busy_it->fd, busy_it->page_no);
        auto curr_it = busy_it++;
        busy_page_ids.push_front(*curr_it);
        busy_page_ids.erase(curr_it);
    }
    check_pages(busy_page_ids);

    for (int fd : fds) {
        PfManager::close_file(fd);
    }
    for (const auto &path : paths) {
        PfManager::destroy_file(path);
    }
}

static constexpr int MAX_FILES = 16;
static constexpr int MAX_PAGES = 128;

class MockPager {
  public:
    uint8_t *get_page(int fd, int page_no) { return &data[fd][page_no * PAGE_SIZE]; }

    void check_disk(int fd, int page_no) {
        EXPECT_TRUE(!PfManager::pager.in_cache({fd, page_no}));
        uint8_t buf[PAGE_SIZE];
        PfPager::read_page(fd, page_no, buf, PAGE_SIZE);
        uint8_t *mock_buf = get_page(fd, page_no);
        EXPECT_EQ(memcmp(buf, mock_buf, PAGE_SIZE), 0);
    }

    void check_cache(int fd, int page_no) {
        EXPECT_TRUE(PfManager::pager.in_cache({fd, page_no}));
        Page *page = PfManager::pager.fetch_page(fd, page_no);
        uint8_t *mock_buf = get_page(fd, page_no);
        EXPECT_EQ(memcmp(page->buf, mock_buf, PAGE_SIZE), 0);
    }

    std::unordered_map<int, std::array<uint8_t, PAGE_SIZE * MAX_PAGES>> data; // fd -> buffer
};

void rand_buf(int size, uint8_t *buf) {
    for (int i = 0; i < size; i++) {
        int rand_ch = rand() & 0xff;
        buf[i] = rand_ch;
    }
}

TEST(PfPagerTest, readwrite) {
    MockPager mock;

    // create files
    std::vector<std::string> filenames;
    std::vector<int> fds;
    for (int i = 0; i < MAX_FILES; i++) {
        std::string filename = std::to_string(i) + ".txt";
        if (PfManager::is_file(filename)) {
            PfManager::destroy_file(filename);
        }
        PfManager::create_file(filename);
        int fd = PfManager::open_file(filename);
        filenames.emplace_back(std::move(filename));
        fds.emplace_back(fd);
        mock.data[fd] = {};
    }

    std::unordered_map<PageId, bool> is_created;
    is_created.reserve(MAX_FILES * MAX_PAGES);
    for (int fd : fds) {
        for (int i = 0; i < MAX_PAGES; i++) {
            is_created[PageId(fd, i)] = false;
        }
    }

    // random access
    for (int i = 0; i < 100000; i++) {
        int file_idx = rand() % MAX_FILES;
        int fd = fds[file_idx];
        int page_no = rand() % MAX_PAGES;
        PageId page_id(fd, page_no);
        uint8_t *mock_buf = mock.get_page(fd, page_no);

        if (!is_created[page_id]) {
            // create page
            Page *page = PfManager::pager.create_page(fd, page_no);
            rand_buf(PAGE_SIZE, page->buf);
            memcpy(mock_buf, page->buf, PAGE_SIZE);
        }

        // get page
        Page *page = PfManager::pager.fetch_page(fd, page_no);
        EXPECT_EQ(memcmp(page->buf, mock_buf, PAGE_SIZE), 0);
        // check equal in cache
        mock.check_cache(fd, page_no);

        // modify
        rand_buf(PAGE_SIZE, page->buf);
        memcpy(mock_buf, page->buf, PAGE_SIZE);
        page->mark_dirty();

        // flush
        if (rand() % 20 == 0) {
            PfManager::pager.flush_page(page);
            mock.check_disk(fd, page_no);
        }
        // flush the entire file
        if (rand() % 200 == 0) {
            PfManager::pager.flush_file(fd);
        }
        // re-open file
        if (rand() % 200 == 0) {
            PfManager::close_file(fd);
            int new_fd = PfManager::open_file(filenames[file_idx]);
            if (new_fd != fd) {
                fds[file_idx] = new_fd;
                mock.data[new_fd] = mock.data[fd];
                mock.data.erase(fd);
            }
        }
    }

    // flush and check disk
    PfManager::pager.flush_all();
    for (int fd : fds) {
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            mock.check_disk(fd, page_no);
        }
    }

    // close and destroy files
    for (int fd : fds) {
        PfManager::close_file(fd);
    }
    for (const auto &path : filenames) {
        PfManager::destroy_file(path);
    }
}
