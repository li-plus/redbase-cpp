#pragma once

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>

class RecordPrinter {
  public:
    RecordPrinter(size_t num_cols_) : num_cols(num_cols_) { assert(num_cols_ > 0); }

    void print_separator() const {
        for (size_t i = 0; i < num_cols; i++) {
            std::cout << '+' << std::string(COL_WIDTH + 2, '-');
        }
        std::cout << "+\n";
    }

    void print_record(const std::vector<std::string> &rec_str) const {
        assert(rec_str.size() == num_cols);
        for (auto col : rec_str) {
            if (col.size() > COL_WIDTH) {
                col = col.substr(0, COL_WIDTH - 3) + "...";
            }
            std::cout << "| " << std::setw(COL_WIDTH) << col << ' ';
        }
        std::cout << "|\n";
    }

    static void print_record_count(size_t num_rec) { std::cout << "Total record(s): " << num_rec << '\n'; }

  private:
    static constexpr size_t COL_WIDTH = 16;
    size_t num_cols;
};
