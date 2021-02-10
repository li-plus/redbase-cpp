#pragma once

#include "pf/pf.h"
#include "defs.h"

struct IxFileHdr {
    int first_free;
    int num_pages;      // # disk pages
    int root_page;      // root page no
    ColType col_type;
    int col_len;
    int btree_order;    // # children per page
    int key_offset;     // offset of key array
    int rid_offset;     // offset of rid array (children array)
    int first_leaf;
    int last_leaf;
};

struct IxPageHdr {
    int next_free;
    int parent;
    int num_key;        // # current keys (always equals to #child - 1)
    int num_child;      // # current children
    bool is_leaf;
    int prev_leaf;      // previous leaf node, effective only when is_leaf is true
    int next_leaf;      // next leaf node, effective only when is_leaf is true
};

struct Iid {
    int page_no;
    int slot_no;

    friend bool operator==(const Iid &x, const Iid &y) {
        return x.page_no == y.page_no && x.slot_no == y.slot_no;
    }

    friend bool operator!=(const Iid &x, const Iid &y) {
        return !(x == y);
    }
};

constexpr int IX_NO_PAGE = -1;
constexpr int IX_FILE_HDR_PAGE = 0;
constexpr int IX_LEAF_HEADER_PAGE = 1;
constexpr int IX_INIT_ROOT_PAGE = 2;
constexpr int IX_INIT_NUM_PAGES = 3;
constexpr int IX_MAX_COL_LEN = 512;
