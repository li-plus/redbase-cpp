#pragma once

#include "rm/bitmap.h"
#include "rm/rm_defs.h"
#include <memory>

struct RmPageHandle {
    RmPageHdr *hdr;
    uint8_t *bitmap;
    uint8_t *slots;
    Page *page;
    const RmFileHdr *fhdr;

    RmPageHandle(const RmFileHdr *fhdr_, Page *page_) : page(page_), fhdr(fhdr_) {
        hdr = (RmPageHdr *)page->buf;
        bitmap = page->buf + sizeof(RmPageHdr);
        slots = bitmap + fhdr->bitmap_size;
    }

    uint8_t *get_slot(int slot_no) const { return slots + slot_no * fhdr->record_size; }
};

class RmFileHandle {
    friend class RmScan;

  public:
    RmFileHdr hdr;
    int fd;

    RmFileHandle(int fd_) {
        fd = fd_;
        PfPager::read_page(fd, RM_FILE_HDR_PAGE, (uint8_t *)&hdr, sizeof(hdr));
    }

    RmFileHandle(const RmFileHandle &other) = delete;

    RmFileHandle &operator=(const RmFileHandle &other) = delete;

    bool is_record(const Rid &rid) const;

    std::unique_ptr<RmRecord> get_record(const Rid &rid) const;

    Rid insert_record(uint8_t *buf);

    void delete_record(const Rid &rid);

    void update_record(const Rid &rid, uint8_t *buf);

  private:
    RmPageHandle fetch_page(int page_no) const;

    RmPageHandle create_page();

    void release_page(RmPageHandle &ph);
};
