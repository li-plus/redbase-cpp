#pragma once

#include "rm_file_handle.h"
#include "rm_defs.h"
#include "bitmap.h"


class RmManager {
public:
    static void create_file(const std::string &filename, int record_size) {
        if (record_size < 1 || record_size > RM_MAX_RECORD_SIZE) {
            throw InvalidRecordSizeError(record_size);
        }
        PfManager::create_file(filename);
        int fd = PfManager::open_file(filename);

        RmFileHdr hdr{};
        hdr.record_size = record_size;
        hdr.num_pages = 1;
        hdr.first_free = RM_NO_PAGE;
        // We have: sizeof(hdr) + (n + 7) / 8 + n * record_size <= PAGE_SIZE
        hdr.num_records_per_page = (BITMAP_WIDTH * (PAGE_SIZE - 1 - (int) sizeof(RmFileHdr)) + 1) /
                                   (1 + record_size * BITMAP_WIDTH);
        hdr.bitmap_size = (hdr.num_records_per_page + BITMAP_WIDTH - 1) / BITMAP_WIDTH;
        PfPager::write_page(fd, RM_FILE_HDR_PAGE, (uint8_t *) &hdr, sizeof(hdr));
        PfManager::close_file(fd);
    }

    static void destroy_file(const std::string &filename) {
        PfManager::destroy_file(filename);
    }

    static std::unique_ptr<RmFileHandle> open_file(const std::string &filename) {
        int fd = PfManager::open_file(filename);
        return std::make_unique<RmFileHandle>(fd);
    }

    static void close_file(const RmFileHandle *fh) {
        PfPager::write_page(fh->fd, RM_FILE_HDR_PAGE, (uint8_t *) &fh->hdr, sizeof(fh->hdr));
        PfManager::close_file(fh->fd);
    }
};
