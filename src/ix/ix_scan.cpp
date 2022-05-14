#include "ix/ix_scan.h"
#include "ix/ix_index_handle.h"
#include <cassert>

void IxScan::next() {
    assert(!is_end());
    IxNodeHandle node = _ih->fetch_node(_iid.page_no);
    assert(node.hdr->is_leaf);
    assert(_iid.slot_no < node.hdr->num_key);
    // increment slot no
    _iid.slot_no++;
    if (_iid.page_no != _ih->hdr.last_leaf && _iid.slot_no == node.hdr->num_key) {
        // go to next leaf
        _iid.slot_no = 0;
        _iid.page_no = node.hdr->next_leaf;
    }
}

Rid IxScan::rid() const { return _ih->get_rid(_iid); }
