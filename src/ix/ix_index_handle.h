#pragma once

#include "ix_defs.h"

static const bool binary_search = false;

static int ix_compare(const uint8_t *a, const uint8_t *b, ColType type, int col_len) {
    switch (type) {
        case TYPE_INT: {
            int ia = *(int *) a;
            int ib = *(int *) b;
            return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
        }
        case TYPE_FLOAT: {
            float fa = *(float *) a;
            float fb = *(float *) b;
            return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
        }
        case TYPE_STRING:
            return memcmp(a, b, col_len);
        default:
            throw InternalError("Unexpected data type");
    }
}

struct IxNodeHandle {
    IxPageHdr *hdr;
    uint8_t *keys;
    Rid *rids;
    Page *page;
    const IxFileHdr *ihdr;

    IxNodeHandle() = default;

    IxNodeHandle(const IxFileHdr *ihdr_, Page *page_) {
        ihdr = ihdr_;
        page = page_;
        hdr = (IxPageHdr *) page->buf;
        keys = page->buf + ihdr->key_offset;
        rids = (Rid *) (page->buf + ihdr->rid_offset);
    }

    uint8_t *get_key(int key_idx) const { return keys + key_idx * ihdr->col_len; }

    Rid *get_rid(int rid_idx) const { return &rids[rid_idx]; }

    int lower_bound(const uint8_t *target) const {
        if (binary_search) {
            int lo = 0, hi = hdr->num_key;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                uint8_t *key_addr = get_key(mid);
                if (ix_compare(target, key_addr, ihdr->col_type, ihdr->col_len) <= 0) {
                    hi = mid;
                } else {
                    lo = mid + 1;
                }
            }
            return lo;
        } else {
            int key_idx = 0;
            while (key_idx < hdr->num_key) {
                uint8_t *key_addr = get_key(key_idx);
                if (ix_compare(target, key_addr, ihdr->col_type, ihdr->col_len) <= 0) {
                    break;
                }
                key_idx++;
            }
            return key_idx;
        }
    }

    int upper_bound(const uint8_t *key) const {
        if (binary_search) {
            int lo = 0, hi = hdr->num_key;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                uint8_t *key_slot = get_key(mid);
                if (ix_compare(key, key_slot, ihdr->col_type, ihdr->col_len) < 0) {
                    hi = mid;
                } else {
                    lo = mid + 1;
                }
            }
            return lo;
        } else {
            int key_idx = 0;
            while (key_idx < hdr->num_key) {
                uint8_t *key_addr = get_key(key_idx);
                if (ix_compare(key, key_addr, ihdr->col_type, ihdr->col_len) < 0) {
                    break;
                }
                key_idx++;
            }
            return key_idx;
        }
    }

    void insert_keys(int pos, const uint8_t *key, int n) {
        uint8_t *key_slot = get_key(pos);
        memmove(key_slot + n * ihdr->col_len, key_slot, (hdr->num_key - pos) * ihdr->col_len);
        memcpy(key_slot, key, n * ihdr->col_len);
        hdr->num_key += n;
    }

    void insert_key(int pos, const uint8_t *key) {
        insert_keys(pos, key, 1);
    }

    void erase_key(int pos) {
        uint8_t *key = get_key(pos);
        memmove(key, key + ihdr->col_len, (hdr->num_key - pos - 1) * ihdr->col_len);
        hdr->num_key--;
    }

    void insert_rids(int pos, const Rid *rid, int n) {
        Rid *rid_slot = get_rid(pos);
        memmove(rid_slot + n, rid_slot, (hdr->num_child - pos) * sizeof(Rid));
        memcpy(rid_slot, rid, n * sizeof(Rid));
        hdr->num_child += n;
    }

    void insert_rid(int pos, const Rid &rid) {
        insert_rids(pos, &rid, 1);
    }

    void erase_rid(int pos) {
        Rid *rid = get_rid(pos);
        memmove(rid, rid + 1, (hdr->num_child - pos - 1) * sizeof(Rid));
        hdr->num_child--;
    }

    int find_child(const IxNodeHandle &child) const {
        int rank;
        for (rank = 0; rank < hdr->num_child; rank++) {
            if (get_rid(rank)->page_no == child.page->id.page_no) {
                break;
            }
        }
        assert(rank < hdr->num_child);
        return rank;
    }
};

class IxIndexHandle {
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
