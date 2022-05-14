#pragma once

#include "ix/ix_defs.h"
#include "ix/ix_index_handle.h"
#include <memory>
#include <string>

class IxManager {
  public:
    static std::string get_index_name(const std::string &filename, int index_no) {
        return filename + '.' + std::to_string(index_no) + ".idx";
    }

    static bool exists(const std::string &filename, int index_no);

    static void create_index(const std::string &filename, int index_no, ColType col_type, int col_len);

    static void destroy_index(const std::string &filename, int index_no);

    static std::unique_ptr<IxIndexHandle> open_index(const std::string &filename, int index_no);

    static void close_index(const IxIndexHandle *ih);
};
