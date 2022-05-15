#pragma once

#include "defs.h"
#include "pf/pf.h"

struct IxFileHdr {
    int first_free;
    int num_pages; // number of disk pages
    int root_page; // root page no
    ColType col_type;
    int col_len;
    int btree_order; // number of children per page
    int key_offset;  // offset of key array
    int rid_offset;  // offset of rid array (children array)
    int first_leaf;
    int last_leaf;

    IxFileHdr() = default;
    IxFileHdr(int first_free_, int num_pages_, int root_page_, ColType col_type_, int col_len_, int btree_order_,
              int key_offset_, int rid_offset_, int first_leaf_, int last_leaf_)
        : first_free(first_free_), num_pages(num_pages_), root_page(root_page_), col_type(col_type_), col_len(col_len_),
          btree_order(btree_order_), key_offset(key_offset_), rid_offset(rid_offset_), first_leaf(first_leaf_),
          last_leaf(last_leaf_) {}
};

struct IxPageHdr {
    int next_free;
    int parent;
    int num_key;   // number of current keys (always equals to #child - 1)
    int num_child; // number of current children
    bool is_leaf;
    int prev_leaf; // previous leaf node, effective only when is_leaf is true
    int next_leaf; // next leaf node, effective only when is_leaf is true

    IxPageHdr() = default;
    IxPageHdr(int next_free_, int parent_, int num_key_, int num_child_, bool is_leaf_, int prev_leaf_, int next_leaf_)
        : next_free(next_free_), parent(parent_), num_key(num_key_), num_child(num_child_), is_leaf(is_leaf_),
          prev_leaf(prev_leaf_), next_leaf(next_leaf_) {}
};

struct Iid {
    int page_no;
    int slot_no;

    Iid() = default;
    Iid(int page_no_, int slot_no_) : page_no(page_no_), slot_no(slot_no_) {}

    friend bool operator==(const Iid &x, const Iid &y) { return x.page_no == y.page_no && x.slot_no == y.slot_no; }
    friend bool operator!=(const Iid &x, const Iid &y) { return !(x == y); }
};

constexpr int IX_NO_PAGE = -1;
constexpr int IX_FILE_HDR_PAGE = 0;
constexpr int IX_LEAF_HEADER_PAGE = 1;
constexpr int IX_INIT_ROOT_PAGE = 2;
constexpr int IX_INIT_NUM_PAGES = 3;
constexpr int IX_MAX_COL_LEN = 512;
