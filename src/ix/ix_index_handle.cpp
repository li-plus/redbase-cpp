#include "ix/ix_index_handle.h"
#include "ix/ix_scan.h"
#include <cassert>

int ix_compare(const uint8_t *a, const uint8_t *b, ColType type, int col_len) {
    switch (type) {
    case TYPE_INT: {
        int ia = *(int *)a;
        int ib = *(int *)b;
        return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
    }
    case TYPE_FLOAT: {
        float fa = *(float *)a;
        float fb = *(float *)b;
        return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
    }
    case TYPE_STRING:
        return memcmp(a, b, col_len);
    default:
        throw InternalError("Unexpected data type");
    }
}

IxNodeHandle::IxNodeHandle(const IxFileHdr *ihdr_, Page *page_) {
    ihdr = ihdr_;
    page = page_;
    hdr = (IxPageHdr *)page->buf;
    keys = page->buf + ihdr->key_offset;
    rids = (Rid *)(page->buf + ihdr->rid_offset);
}

int IxNodeHandle::lower_bound(const uint8_t *target) const {
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

int IxNodeHandle::upper_bound(const uint8_t *key) const {
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

void IxNodeHandle::insert_keys(int pos, const uint8_t *key, int n) {
    uint8_t *key_slot = get_key(pos);
    memmove(key_slot + n * ihdr->col_len, key_slot, (hdr->num_key - pos) * ihdr->col_len);
    memcpy(key_slot, key, n * ihdr->col_len);
    hdr->num_key += n;
}

void IxNodeHandle::insert_key(int pos, const uint8_t *key) { insert_keys(pos, key, 1); }

void IxNodeHandle::erase_key(int pos) {
    uint8_t *key = get_key(pos);
    memmove(key, key + ihdr->col_len, (hdr->num_key - pos - 1) * ihdr->col_len);
    hdr->num_key--;
}

void IxNodeHandle::insert_rids(int pos, const Rid *rid, int n) {
    Rid *rid_slot = get_rid(pos);
    memmove(rid_slot + n, rid_slot, (hdr->num_child - pos) * sizeof(Rid));
    memcpy(rid_slot, rid, n * sizeof(Rid));
    hdr->num_child += n;
}

void IxNodeHandle::insert_rid(int pos, const Rid &rid) { insert_rids(pos, &rid, 1); }

void IxNodeHandle::erase_rid(int pos) {
    Rid *rid = get_rid(pos);
    memmove(rid, rid + 1, (hdr->num_child - pos - 1) * sizeof(Rid));
    hdr->num_child--;
}

int IxNodeHandle::find_child(const IxNodeHandle &child) const {
    int rank;
    for (rank = 0; rank < hdr->num_child; rank++) {
        if (get_rid(rank)->page_no == child.page->id.page_no) {
            break;
        }
    }
    assert(rank < hdr->num_child);
    return rank;
}

IxIndexHandle::IxIndexHandle(int fd_) {
    fd = fd_;
    PfPager::read_page(fd, IX_FILE_HDR_PAGE, (uint8_t *)&hdr, sizeof(hdr));
}

void IxIndexHandle::insert_entry(const uint8_t *key, const Rid &rid) {
    Iid iid = upper_bound(key);
    IxNodeHandle node = fetch_node(iid.page_no);
    node.page->mark_dirty();
    // We need to insert at iid.slot_no
    node.insert_key(iid.slot_no, key);
    node.insert_rid(iid.slot_no, rid);
    // Maintain parent's max key
    if (iid.page_no == hdr.last_leaf && iid.slot_no == node.hdr->num_key - 1) {
        // Max key updated
        maintain_parent(node);
    }
    // Solve overflow
    while (node.hdr->num_child > hdr.btree_order) {
        // If leaf node is overflowed, we need to split it
        if (node.hdr->parent == IX_NO_PAGE) {
            // If current page is root node, allocate new root
            IxNodeHandle root = create_node();
            *root.hdr = IxPageHdr(IX_NO_PAGE, IX_NO_PAGE, 0, 0, false, IX_NO_PAGE, IX_NO_PAGE);
            // Insert current node's key & rid
            Rid curr_rid(node.page->id.page_no, -1);
            root.insert_rid(0, curr_rid);
            root.insert_key(0, node.get_key(node.hdr->num_key - 1));
            // update current node's parent
            node.hdr->parent = root.page->id.page_no;
            // update global root page
            hdr.root_page = root.page->id.page_no;
        }
        // Allocate brother node
        IxNodeHandle bro = create_node();
        *bro.hdr = IxPageHdr(IX_NO_PAGE,
                             node.hdr->parent, // They have the same parent
                             0, 0,
                             node.hdr->is_leaf, // Brother node is leaf only if current node is leaf.
                             IX_NO_PAGE, IX_NO_PAGE);
        if (bro.hdr->is_leaf) {
            // maintain brother node's leaf pointer
            bro.hdr->next_leaf = node.hdr->next_leaf;
            bro.hdr->prev_leaf = node.page->id.page_no;
            // Let original next node's prev = brother node
            IxNodeHandle next = fetch_node(node.hdr->next_leaf);
            next.page->mark_dirty();
            next.hdr->prev_leaf = bro.page->id.page_no;
            // curr's next = brother node
            node.hdr->next_leaf = bro.page->id.page_no;
        }
        // Split at middle position
        int split_idx = node.hdr->num_child / 2;
        // Keys in [0, split_idx) stay in current node, [split_idx, curr_keys) go to brother node
        int num_transfer = node.hdr->num_key - split_idx;
        bro.insert_keys(0, node.get_key(split_idx), num_transfer);
        bro.insert_rids(0, node.get_rid(split_idx), num_transfer);
        node.hdr->num_key = split_idx;
        node.hdr->num_child = split_idx;
        // Update children's parent
        for (int child_idx = 0; child_idx < bro.hdr->num_child; child_idx++) {
            maintain_child(bro, child_idx);
        }
        // Copy the last key up to its parent
        uint8_t *popup_key = node.get_key(split_idx - 1);
        // Load parent node
        IxNodeHandle parent = fetch_node(node.hdr->parent);
        parent.page->mark_dirty();
        // Find the rank of current node in its parent
        int child_idx = parent.find_child(node);
        // Insert popup key into parent
        parent.insert_key(child_idx, popup_key);
        Rid bro_rid(bro.page->id.page_no, -1);
        parent.insert_rid(child_idx + 1, bro_rid);
        // Update global last_leaf if needed
        if (hdr.last_leaf == node.page->id.page_no) {
            hdr.last_leaf = bro.page->id.page_no;
        }
        // Go to its parent
        node = parent;
    }
}

void IxIndexHandle::delete_entry(const uint8_t *key, const Rid &rid) {
    Iid lower = lower_bound(key);
    Iid upper = upper_bound(key);
    for (IxScan scan(this, lower, upper); !scan.is_end(); scan.next()) {
        // load btree node
        IxNodeHandle node = fetch_node(scan.iid().page_no);
        assert(node.hdr->is_leaf);
        Rid *curr_rid = node.get_rid(scan.iid().slot_no);
        if (*curr_rid != rid) {
            continue;
        }
        // Found the entry with the given rid, delete it
        node.page->mark_dirty();
        node.erase_key(scan.iid().slot_no);
        node.erase_rid(scan.iid().slot_no);
        // Update its parent's key to the node's new last key
        maintain_parent(node);
        // Solve underflow
        while (node.hdr->num_child < (hdr.btree_order + 1) / 2) {
            if (node.hdr->parent == IX_NO_PAGE) {
                // If current node is root node, underflow is permitted
                if (!node.hdr->is_leaf && node.hdr->num_key <= 1) {
                    // If root node is not leaf and it is empty, delete the root
                    int new_root_page = node.get_rid(0)->page_no;
                    // Load new root and set its parent to NO_PAGE
                    IxNodeHandle new_root = fetch_node(new_root_page);
                    new_root.page->mark_dirty();
                    new_root.hdr->parent = IX_NO_PAGE;
                    // Update global root
                    hdr.root_page = new_root_page;
                    // Free current page
                    release_node(node);
                }
                break;
            }
            // Load parent node
            IxNodeHandle parent = fetch_node(node.hdr->parent);
            parent.page->mark_dirty();
            // Find the rank of this child in its parent
            int child_idx = parent.find_child(node);
            if (0 < child_idx) {
                // current node has left brother, load it
                IxNodeHandle bro = fetch_node(parent.get_rid(child_idx - 1)->page_no);
                if (bro.hdr->num_child > (hdr.btree_order + 1) / 2) {
                    // If left brother is rich, borrow one node from it
                    bro.page->mark_dirty();
                    node.insert_key(0, bro.get_key(bro.hdr->num_key - 1));
                    node.insert_rid(0, *bro.get_rid(bro.hdr->num_child - 1));
                    bro.erase_key(bro.hdr->num_key - 1);
                    bro.erase_rid(bro.hdr->num_child - 1);
                    // Maintain parent's key as the node's max key
                    maintain_parent(bro);
                    // Maintain first child's parent
                    maintain_child(node, 0);
                    // underflow is solved
                    break;
                }
            }
            if (child_idx + 1 < parent.hdr->num_child) {
                // current node has right brother, load it
                IxNodeHandle bro = fetch_node(parent.get_rid(child_idx + 1)->page_no);
                if (bro.hdr->num_child > (hdr.btree_order + 1) / 2) {
                    // If right brother is rich, borrow one node from it
                    bro.page->mark_dirty();
                    node.insert_key(node.hdr->num_key, bro.get_key(0));
                    node.insert_rid(node.hdr->num_child, *bro.get_rid(0));
                    bro.erase_key(0);
                    bro.erase_rid(0);
                    // Maintain parent's key as the node's max key
                    maintain_parent(node);
                    // Maintain last child's parent
                    maintain_child(node, node.hdr->num_child - 1);
                    // Underflow is solved
                    break;
                }
            }
            // neither brothers is rich, need to merge
            if (0 < child_idx) {
                // merge with left brother, transfer all children of current node to left brother
                IxNodeHandle bro = fetch_node(parent.get_rid(child_idx - 1)->page_no);
                bro.page->mark_dirty();
                bro.insert_keys(bro.hdr->num_key, node.get_key(0), node.hdr->num_key);
                bro.insert_rids(bro.hdr->num_child, node.get_rid(0), node.hdr->num_child);
                // Maintain left brother's children
                for (int i = bro.hdr->num_child - node.hdr->num_child; i < bro.hdr->num_child; i++) {
                    maintain_child(bro, i);
                }
                parent.erase_key(child_idx);
                parent.erase_rid(child_idx);
                maintain_parent(bro);
                // Maintain leaf list
                if (node.hdr->is_leaf) {
                    erase_leaf(node);
                }
                // Update global last-leaf
                if (hdr.last_leaf == node.page->id.page_no) {
                    hdr.last_leaf = bro.page->id.page_no;
                }
                // Free current page
                release_node(node);
            } else {
                assert(child_idx + 1 < parent.hdr->num_child);
                // merge with right brother, transfer all children of right brother to current node
                IxNodeHandle bro = fetch_node(parent.get_rid(child_idx + 1)->page_no);
                bro.page->mark_dirty();
                // Transfer all right brother's valid rid to current node
                node.insert_rids(node.hdr->num_child, bro.get_rid(0), bro.hdr->num_child);
                node.insert_keys(node.hdr->num_key, bro.get_key(0), bro.hdr->num_key);
                // Maintain current node's children
                for (int i = node.hdr->num_child - bro.hdr->num_child; i < node.hdr->num_child; i++) {
                    maintain_child(node, i);
                }
                parent.erase_rid(child_idx + 1);
                parent.erase_key(child_idx);
                // Maintain parent's key as the node's max key
                maintain_parent(node);
                // Maintain leaf list
                if (bro.hdr->is_leaf) {
                    erase_leaf(bro);
                }
                // Update global last leaf
                if (hdr.last_leaf == bro.page->id.page_no) {
                    hdr.last_leaf = node.page->id.page_no;
                }
                // Free right brother page
                release_node(bro);
            }
            node = parent;
        }
        return;
    }
    throw IndexEntryNotFoundError();
}

Rid IxIndexHandle::get_rid(const Iid &iid) const {
    IxNodeHandle node = fetch_node(iid.page_no);
    if (iid.slot_no >= node.hdr->num_child) {
        throw IndexEntryNotFoundError();
    }
    return *node.get_rid(iid.slot_no);
}

Iid IxIndexHandle::lower_bound(const uint8_t *key) const {
    IxNodeHandle node = fetch_node(hdr.root_page);
    // Travel through inner nodes
    while (!node.hdr->is_leaf) {
        int key_idx = node.lower_bound(key);
        if (key_idx >= node.hdr->num_key) {
            return leaf_end();
        }
        Rid *child = node.get_rid(key_idx);
        node = fetch_node(child->page_no);
    }
    // Now we come to a leaf node, we do a sequential search
    int key_idx = node.lower_bound(key);
    Iid iid(node.page->id.page_no, key_idx);
    return iid;
}

Iid IxIndexHandle::upper_bound(const uint8_t *key) const {
    IxNodeHandle node = fetch_node(hdr.root_page);
    // Travel through inner nodes
    while (!node.hdr->is_leaf) {
        int key_idx = node.upper_bound(key);
        if (key_idx >= node.hdr->num_key) {
            return leaf_end();
        }
        Rid *child = node.get_rid(key_idx);
        node = fetch_node(child->page_no);
    }
    // Now we come to a leaf node, we do a sequential search
    int key_idx = node.upper_bound(key);
    Iid iid(node.page->id.page_no, key_idx);
    return iid;
}

Iid IxIndexHandle::leaf_end() const {
    IxNodeHandle node = fetch_node(hdr.last_leaf);
    Iid iid(hdr.last_leaf, node.hdr->num_key);
    return iid;
}

Iid IxIndexHandle::leaf_begin() const {
    Iid iid(hdr.first_leaf, 0);
    return iid;
}

IxNodeHandle IxIndexHandle::create_node() {
    Page *page;
    IxNodeHandle node;
    if (hdr.first_free == IX_NO_PAGE) {
        page = PfManager::pager.create_page(fd, hdr.num_pages);
        hdr.num_pages++;
        node = IxNodeHandle(&hdr, page);
    } else {
        page = PfManager::pager.fetch_page(fd, hdr.first_free);
        node = IxNodeHandle(&hdr, page);
        hdr.first_free = node.hdr->next_free;
    }
    page->mark_dirty();
    return node;
}

IxNodeHandle IxIndexHandle::fetch_node(int page_no) const {
    assert(page_no < hdr.num_pages);
    Page *page = PfManager::pager.fetch_page(fd, page_no);
    IxNodeHandle node(&hdr, page);
    return node;
}

void IxIndexHandle::maintain_parent(const IxNodeHandle &node) {
    IxNodeHandle curr = node;
    while (curr.hdr->parent != IX_NO_PAGE) {
        // Load its parent
        IxNodeHandle parent = fetch_node(curr.hdr->parent);
        int rank = parent.find_child(curr);
        uint8_t *parent_key = parent.get_key(rank);
        uint8_t *child_max_key = curr.get_key(curr.hdr->num_key - 1);
        if (memcmp(parent_key, child_max_key, hdr.col_len) == 0) {
            break;
        }
        parent.page->mark_dirty();
        memcpy(parent_key, child_max_key, hdr.col_len);
        curr = parent;
    }
}

void IxIndexHandle::erase_leaf(IxNodeHandle &leaf) {
    assert(leaf.hdr->is_leaf);
    IxNodeHandle prev = fetch_node(leaf.hdr->prev_leaf);
    prev.page->mark_dirty();
    prev.hdr->next_leaf = leaf.hdr->next_leaf;

    IxNodeHandle next = fetch_node(leaf.hdr->next_leaf);
    next.page->mark_dirty();
    next.hdr->prev_leaf = leaf.hdr->prev_leaf;
}

void IxIndexHandle::release_node(IxNodeHandle &node) {
    node.hdr->next_free = hdr.first_free;
    hdr.first_free = node.page->id.page_no;
}

void IxIndexHandle::maintain_child(IxNodeHandle &node, int child_idx) {
    if (!node.hdr->is_leaf) {
        // Current node is inner node, load its child and set its parent to current node
        int child_page_no = node.get_rid(child_idx)->page_no;
        IxNodeHandle child = fetch_node(child_page_no);
        child.page->mark_dirty();
        child.hdr->parent = node.page->id.page_no;
    }
}
