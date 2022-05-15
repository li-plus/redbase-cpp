#include "rm/rm.h"
#include <gtest/gtest.h>

void rand_buf(int size, uint8_t *out_buf) {
    for (int i = 0; i < size; i++) {
        out_buf[i] = rand() & 0xff;
    }
}

struct rid_hash_t {
    size_t operator()(const Rid &rid) const { return (rid.page_no << 16) | rid.slot_no; }
};

struct rid_equal_t {
    bool operator()(const Rid &x, const Rid &y) const { return x.page_no == y.page_no && x.slot_no == y.slot_no; }
};

void check_equal(const RmFileHandle *fh, const std::unordered_map<Rid, std::string, rid_hash_t, rid_equal_t> &mock) {
    // Test all records
    for (auto &entry : mock) {
        Rid rid = entry.first;
        auto mock_buf = (uint8_t *)entry.second.c_str();
        auto rec = fh->get_record(rid);
        EXPECT_EQ(memcmp(mock_buf, rec->data, fh->hdr.record_size), 0);
    }
    // Randomly get record
    for (int i = 0; i < 10; i++) {
        Rid rid(1 + rand() % (fh->hdr.num_pages - 1), rand() % fh->hdr.num_records_per_page);
        bool mock_exist = mock.count(rid) > 0;
        bool rm_exist = fh->is_record(rid);
        EXPECT_EQ(rm_exist, mock_exist);
    }
    // Test RM scan
    size_t num_records = 0;
    for (RmScan scan(fh); !scan.is_end(); scan.next()) {
        EXPECT_GT(mock.count(scan.rid()), 0);
        auto rec = fh->get_record(scan.rid());
        EXPECT_EQ(memcmp(rec->data, mock.at(scan.rid()).c_str(), fh->hdr.record_size), 0);
        num_records++;
    }
    EXPECT_EQ(num_records, mock.size());
}

std::ostream &operator<<(std::ostream &os, const Rid &rid) {
    return os << '(' << rid.page_no << ", " << rid.slot_no << ')';
}

TEST(rm, basic) {
    srand((unsigned)time(nullptr));

    std::unordered_map<Rid, std::string, rid_hash_t, rid_equal_t> mock;

    std::string filename = "abc.txt";

    int record_size = 4 + rand() % 256;
    // test files
    {
        if (PfManager::is_file(filename)) {
            PfManager::destroy_file(filename);
        }
        RmManager::create_file(filename, record_size);
        auto fh = RmManager::open_file(filename);
        EXPECT_EQ(fh->hdr.record_size, record_size);
        EXPECT_EQ(fh->hdr.first_free, RM_NO_PAGE);
        EXPECT_EQ(fh->hdr.num_pages, 1);
        int max_bytes =
            fh->hdr.record_size * fh->hdr.num_records_per_page + fh->hdr.bitmap_size + (int)sizeof(RmPageHdr);
        EXPECT_LE(max_bytes, PAGE_SIZE);
        int rand_val = rand();
        fh->hdr.num_pages = rand_val;
        RmManager::close_file(fh.get());
        // reopen file
        fh = RmManager::open_file(filename);
        EXPECT_EQ(fh->hdr.num_pages, rand_val);
        RmManager::close_file(fh.get());
        RmManager::destroy_file(filename);
    }
    // test pages
    RmManager::create_file(filename, record_size);
    auto fh = RmManager::open_file(filename);

    uint8_t write_buf[PAGE_SIZE];
    size_t add_cnt = 0;
    size_t upd_cnt = 0;
    size_t del_cnt = 0;
    for (int round = 0; round < 10000; round++) {
        double insert_prob = 1. - mock.size() / 2500.;
        double dice = rand() * 1. / RAND_MAX;
        if (mock.empty() || dice < insert_prob) {
            rand_buf(fh->hdr.record_size, write_buf);
            Rid rid = fh->insert_record(write_buf);
            mock[rid] = std::string((char *)write_buf, fh->hdr.record_size);
            add_cnt++;
            //            std::cout << "insert " << rid << '\n';
        } else {
            // update or erase random rid
            int rid_idx = rand() % mock.size();
            auto it = mock.begin();
            for (int i = 0; i < rid_idx; i++) {
                it++;
            }
            auto rid = it->first;
            if (rand() % 2 == 0) {
                // update
                rand_buf(fh->hdr.record_size, write_buf);
                fh->update_record(rid, write_buf);
                mock[rid] = std::string((char *)write_buf, fh->hdr.record_size);
                upd_cnt++;
                //                std::cout << "update " << rid << '\n';
            } else {
                // erase
                fh->delete_record(rid);
                mock.erase(rid);
                del_cnt++;
                //                std::cout << "delete " << rid << '\n';
            }
        }
        // Randomly re-open file
        if (round % 500 == 0) {
            RmManager::close_file(fh.get());
            fh = RmManager::open_file(filename);
        }
        check_equal(fh.get(), mock);
    }
    EXPECT_EQ(mock.size(), add_cnt - del_cnt);
    std::cout << "insert " << add_cnt << '\n' << "delete " << del_cnt << '\n' << "update " << upd_cnt << '\n';
    // clean up
    RmManager::close_file(fh.get());
    RmManager::destroy_file(filename);
}