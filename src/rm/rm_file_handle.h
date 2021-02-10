#pragma once

#include "rm_defs.h"
#include "bitmap.h"
#include <memory>

struct RmPageHandle {
    RmPageHdr *hdr;
    Buffer bitmap;
    Buffer slots;
    Page *page;
    const RmFileHdr *fhdr;

    RmPageHandle(const RmFileHdr *fhdr_, Page *page_) : page(page_), fhdr(fhdr_) {
        hdr = (RmPageHdr *) page->buf;
        bitmap = page->buf + sizeof(RmPageHdr);
        slots = bitmap + fhdr->bitmap_size;
    }

    Buffer get_slot(int slot_no) const {
        return slots + slot_no * fhdr->record_size;
    }
};

class RmFileHandle {
    friend class RmScan;

public:
    RmFileHdr hdr;
    int fd;

    RmFileHandle(int fd_) {
        fd = fd_;
        PfPager::read_page(fd, RM_FILE_HDR_PAGE, (Buffer) &hdr, sizeof(hdr));
    }

    RmFileHandle(const RmFileHandle &other) = delete;

    RmFileHandle &operator=(const RmFileHandle &other) = delete;

    bool is_record(const Rid &rid) const {
        RmPageHandle ph = fetch_page(rid.page_no);
        return Bitmap::test(ph.bitmap, rid.slot_no);
    }

    std::shared_ptr<RmRecord> get_record(const Rid &rid) const;

    Rid insert_record(Buffer buf);

    void delete_record(const Rid &rid);

    void update_record(const Rid &rid, Buffer buf);

private:
    RmPageHandle fetch_page(int page_no) const;

    RmPageHandle create_page();

    void release_page(RmPageHandle &ph);
};
