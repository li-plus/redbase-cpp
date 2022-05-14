#pragma once

#include "ix/ix_defs.h"

static const bool binary_search = false;

int ix_compare(const uint8_t *a, const uint8_t *b, ColType type, int col_len);

struct IxNodeHandle {
    IxPageHdr *hdr;
    uint8_t *keys;
    Rid *rids;
    Page *page;
    const IxFileHdr *ihdr;

    IxNodeHandle() = default;

    IxNodeHandle(const IxFileHdr *ihdr_, Page *page_);

    uint8_t *get_key(int key_idx) const { return keys + key_idx * ihdr->col_len; }

    Rid *get_rid(int rid_idx) const { return &rids[rid_idx]; }

    int lower_bound(const uint8_t *target) const;
    int upper_bound(const uint8_t *key) const;

    void insert_keys(int pos, const uint8_t *key, int n);
    void insert_key(int pos, const uint8_t *key);
    void erase_key(int pos);

    void insert_rids(int pos, const Rid *rid, int n);
    void insert_rid(int pos, const Rid &rid);
    void erase_rid(int pos);

    int find_child(const IxNodeHandle &child) const;
};

class IxIndexHandle {
    friend class IxTest;
    friend class IxScan;

  public:
    int fd;
    IxFileHdr hdr;

    IxIndexHandle(int fd_);

    void insert_entry(const uint8_t *key, const Rid &rid);

    void delete_entry(const uint8_t *key, const Rid &rid);

    Rid get_rid(const Iid &iid) const;

    Iid lower_bound(const uint8_t *key) const;

    Iid upper_bound(const uint8_t *key) const;

    Iid leaf_end() const;

    Iid leaf_begin() const;

  private:
    IxNodeHandle fetch_node(int page_no) const;

    IxNodeHandle create_node();

    void maintain_parent(const IxNodeHandle &node);

    void erase_leaf(IxNodeHandle &leaf);

    void release_node(IxNodeHandle &node);

    void maintain_child(IxNodeHandle &node, int child_idx);
};
