#pragma once

#include "ix_index_handle.h"
#include "ix_defs.h"
#include <string>
#include <memory>

class IxManager {
public:
    static std::string get_index_name(const std::string &filename, int index_no) {
        return filename + '.' + std::to_string(index_no) + ".idx";
    }

    static bool exists(const std::string &filename, int index_no) {
        auto ix_name = get_index_name(filename, index_no);
        return PfManager::is_file(ix_name);
    }

    static void create_index(const std::string &filename, int index_no, ColType col_type, int col_len) {
        std::string ix_name = get_index_name(filename, index_no);
        assert(index_no >= 0);
        // Create index file
        PfManager::create_file(ix_name);
        // Open index file
        int fd = PfManager::open_file(ix_name);
        // Create file header and write to file
        // Theoretically we have: |page_hdr| + (|attr| + |rid|) * n <= PAGE_SIZE
        // but we reserve one slot for convenient inserting and deleting, i.e.
        // |page_hdr| + (|attr| + |rid|) * (n + 1) <= PAGE_SIZE
        if (col_len > IX_MAX_COL_LEN) { throw InvalidColLengthError(col_len); }
        int btree_order = (int) ((PAGE_SIZE - sizeof(IxPageHdr)) / (col_len + sizeof(Rid)) - 1);
        assert(btree_order > 2);
        int key_offset = sizeof(IxPageHdr);
        int rid_offset = key_offset + (btree_order + 1) * col_len;

        IxFileHdr fhdr = {
                .first_free = IX_NO_PAGE,
                .num_pages = IX_INIT_NUM_PAGES,
                .root_page = IX_INIT_ROOT_PAGE,
                .col_type = col_type,
                .col_len = col_len,
                .btree_order = btree_order,
                .key_offset = key_offset,
                .rid_offset = rid_offset,
                .first_leaf = IX_INIT_ROOT_PAGE,
                .last_leaf = IX_INIT_ROOT_PAGE,
        };
        static uint8_t page_buf[PAGE_SIZE];
        PfPager::write_page(fd, IX_FILE_HDR_PAGE, (const uint8_t *) &fhdr, sizeof(fhdr));
        // Create leaf list header page and write to file
        {
            auto phdr = (IxPageHdr *) page_buf;
            *phdr = {
                    .next_free = IX_NO_PAGE,
                    .parent = IX_NO_PAGE,
                    .num_key = 0,
                    .num_child = 0,
                    .is_leaf = true,
                    .prev_leaf = IX_INIT_ROOT_PAGE,
                    .next_leaf = IX_INIT_ROOT_PAGE,
            };
            PfPager::write_page(fd, IX_LEAF_HEADER_PAGE, page_buf, PAGE_SIZE);
        }
        // Create root node and write to file
        {
            auto phdr = (IxPageHdr *) page_buf;
            *phdr = {
                    .next_free = IX_NO_PAGE,
                    .parent = IX_NO_PAGE,
                    .num_key = 0,
                    .num_child = 0,
                    .is_leaf = true,
                    .prev_leaf = IX_LEAF_HEADER_PAGE,
                    .next_leaf = IX_LEAF_HEADER_PAGE,
            };
            // Must write PAGE_SIZE here in case of future fetch_node()
            PfPager::write_page(fd, IX_INIT_ROOT_PAGE, page_buf, PAGE_SIZE);
        }
        // Close index file
        PfManager::close_file(fd);
    }

    static void destroy_index(const std::string &filename, int index_no) {
        std::string ix_name = get_index_name(filename, index_no);
        PfManager::destroy_file(ix_name);
    }

    static std::unique_ptr<IxIndexHandle> open_index(const std::string &filename, int index_no) {
        std::string ix_name = get_index_name(filename, index_no);
        int fd = PfManager::open_file(ix_name);
        return std::make_unique<IxIndexHandle>(fd);
    }

    static void close_index(const IxIndexHandle *ih) {
        PfPager::write_page(ih->fd, IX_FILE_HDR_PAGE, (const uint8_t *) &ih->hdr, sizeof(ih->hdr));
        PfManager::close_file(ih->fd);
    }
};
