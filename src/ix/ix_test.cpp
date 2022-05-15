#include "ix/ix.h"
#include <gtest/gtest.h>

class IxTest : public ::testing::Test {
  public:
    void check_tree(const IxIndexHandle *ih, int root_page) {
        IxNodeHandle node = ih->fetch_node(root_page);
        if (node.hdr->is_leaf) {
            return;
        }
        for (int i = 0; i < node.hdr->num_child; i++) {
            IxNodeHandle child = ih->fetch_node(node.get_rid(i)->page_no);
            // check parent
            EXPECT_EQ(child.hdr->parent, root_page);
            // check last key
            EXPECT_EQ(memcmp(node.get_key(i), child.get_key(child.hdr->num_key - 1), ih->hdr.col_len), 0);
            check_tree(ih, node.get_rid(i)->page_no);
        }
    }

    void check_leaf(const IxIndexHandle *ih) {
        // check leaf list
        int leaf_no = ih->hdr.first_leaf;
        while (leaf_no != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle curr = ih->fetch_node(leaf_no);
            IxNodeHandle prev = ih->fetch_node(curr.hdr->prev_leaf);
            IxNodeHandle next = ih->fetch_node(curr.hdr->next_leaf);
            // Ensure prev->next == curr && next->prev == curr
            EXPECT_EQ(prev.hdr->next_leaf, leaf_no);
            EXPECT_EQ(next.hdr->prev_leaf, leaf_no);
            leaf_no = curr.hdr->next_leaf;
        }
    }

    void check_equal(const IxIndexHandle *ih, const std::multimap<int, Rid> &mock) {
        check_tree(ih, ih->hdr.root_page);
        check_leaf(ih);
        for (auto &entry : mock) {
            int mock_key = entry.first;
            // test lower bound
            {
                auto mock_lower = mock.lower_bound(mock_key);
                Iid iid = ih->lower_bound((const uint8_t *)&mock_key);
                Rid rid = ih->get_rid(iid);
                EXPECT_EQ(rid, mock_lower->second);
            }
            // test upper bound
            {
                auto mock_upper = mock.upper_bound(mock_key);
                Iid iid = ih->upper_bound((const uint8_t *)&mock_key);
                if (mock_upper == mock.end()) {
                    EXPECT_EQ(iid, ih->leaf_end());
                } else {
                    Rid rid = ih->get_rid(iid);
                    EXPECT_EQ(rid, mock_upper->second);
                }
            }
        }
        // test scan
        IxScan scan(ih, ih->leaf_begin(), ih->leaf_end());
        auto it = mock.begin();
        while (!scan.is_end() && it != mock.end()) {
            Rid mock_rid = it->second;
            Rid rid = scan.rid();
            EXPECT_EQ(rid, mock_rid);
            it++;
            scan.next();
        }
        EXPECT_TRUE(scan.is_end());
        EXPECT_EQ(it, mock.end());
    }

    void print_btree(IxIndexHandle &ih, int root_page, int offset) {
        IxNodeHandle node = ih.fetch_node(root_page);
        for (int i = node.hdr->num_child - 1; i > -1; i--) {
            // print key
            std::cout << std::string(offset, ' ') << *(int *)node.get_key(i) << std::endl;
            // print child
            if (!node.hdr->is_leaf) {
                print_btree(ih, node.get_rid(i)->page_no, offset + 4);
            }
        }
    }

    void test_ix_insert_delete(int order, int round) {
        std::string filename = "abc";
        int index_no = 0;
        if (IxManager::exists(filename, index_no)) {
            IxManager::destroy_index(filename, index_no);
        }
        IxManager::create_index(filename, index_no, TYPE_INT, sizeof(int));
        auto ih = IxManager::open_index(filename, index_no);
        if (order > 2 && order <= ih->hdr.btree_order) {
            ih->hdr.btree_order = order;
        }
        std::multimap<int, Rid> mock;
        for (int i = 0; i < round; i++) {
            int rand_key = rand() % round;
            Rid rand_val(rand(), rand());
            ih->insert_entry((const uint8_t *)&rand_key, rand_val);
            mock.insert(std::make_pair(rand_key, rand_val));
            if (round % 500 == 0) {
                IxManager::close_index(ih.get());
                ih = IxManager::open_index(filename, index_no);
            }
        }
        std::cout << "Insert " << round << std::endl;
        //    print_btree(ih, ih.hdr.root_page, 0);
        check_equal(ih.get(), mock);
        for (int i = 0; i < round; i++) {
            auto it = mock.begin();
            int key = it->first;
            Rid rid = it->second;
            ih->delete_entry((const uint8_t *)&key, rid);
            mock.erase(it);
            if (round % 500 == 0) {
                IxManager::close_index(ih.get());
                ih = IxManager::open_index(filename, index_no);
            }
        }
        std::cout << "delete " << round << std::endl;
        check_equal(ih.get(), mock);
        IxManager::close_index(ih.get());
        IxManager::destroy_index(filename, index_no);
    }

    void test_ix(int order, int round) {
        std::string filename = "abc";
        int index_no = 0;
        if (IxManager::exists(filename, index_no)) {
            IxManager::destroy_index(filename, index_no);
        }
        IxManager::create_index(filename, index_no, TYPE_INT, sizeof(int));
        auto ih = IxManager::open_index(filename, index_no);
        if (order >= 2 && order <= ih->hdr.btree_order) {
            ih->hdr.btree_order = order;
        }
        int add_cnt = 0;
        int del_cnt = 0;
        std::multimap<int, Rid> mock;
        for (int i = 0; i < round; i++) {
            double dice = rand() * 1. / RAND_MAX;
            double insert_prob = 1. - mock.size() / (0.5 * round);
            if (mock.empty() || dice < insert_prob) {
                // Insert
                int rand_key = rand() % round;
                Rid rand_val(rand(), rand());
                ih->insert_entry((const uint8_t *)&rand_key, rand_val);
                mock.insert(std::make_pair(rand_key, rand_val));
                add_cnt++;
            } else {
                // Delete
                int rand_idx = rand() % mock.size();
                auto it = mock.begin();
                for (int k = 0; k < rand_idx; k++) {
                    it++;
                }
                int key = it->first;
                Rid rid = it->second;
                ih->delete_entry((const uint8_t *)&key, rid);
                mock.erase(it);
                del_cnt++;
            }
            // Randomly re-open file
            if (round % 500 == 0) {
                IxManager::close_index(ih.get());
                ih = IxManager::open_index(filename, index_no);
            }
        }
        //    print_btree(ih, ih.hdr.root_page, 0);
        std::cout << "Insert " << add_cnt << '\n' << "Delete " << del_cnt << '\n';
        while (!mock.empty()) {
            int rand_idx = rand() % mock.size();
            auto it = mock.begin();
            for (int k = 0; k < rand_idx; k++) {
                it++;
            }
            int key = it->first;
            Rid rid = it->second;
            ih->delete_entry((const uint8_t *)&key, rid);
            mock.erase(it);
            // Randomly re-open file
            if (round % 500 == 0) {
                IxManager::close_index(ih.get());
                ih = IxManager::open_index(filename, index_no);
            }
        }
        check_equal(ih.get(), mock);
        IxManager::close_index(ih.get());
        IxManager::destroy_index(filename, index_no);
    }
};

TEST_F(IxTest, basic) {
    srand((unsigned)time(nullptr));
    // init
    test_ix_insert_delete(3, 1000);
    test_ix(4, 1000);
    test_ix(-1, 100000);
}
