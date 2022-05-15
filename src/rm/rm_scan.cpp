#include "rm/rm_scan.h"
#include "rm/rm_file_handle.h"
#include <cassert>

RmScan::RmScan(const RmFileHandle *fh) : _fh(fh) {
    _rid = Rid(RM_FIRST_RECORD_PAGE, -1);
    next();
}

void RmScan::next() {
    assert(!is_end());
    while (_rid.page_no < _fh->hdr.num_pages) {
        RmPageHandle ph = _fh->fetch_page(_rid.page_no);
        _rid.slot_no = Bitmap::next_bit(true, ph.bitmap, _fh->hdr.num_records_per_page, _rid.slot_no);
        if (_rid.slot_no < _fh->hdr.num_records_per_page) {
            return;
        }
        _rid.slot_no = -1;
        _rid.page_no++;
    }
    // next record not found
    _rid.page_no = RM_NO_PAGE;
}
