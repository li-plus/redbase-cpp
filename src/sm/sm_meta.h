#pragma once

#include "error.h"
#include "sm/sm_defs.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct ColMeta {
    std::string tab_name;
    std::string name;
    ColType type;
    int len;
    int offset;
    bool index;

    ColMeta() = default;
    ColMeta(std::string tab_name_, std::string name_, ColType type_, int len_, int offset_, bool index_)
        : tab_name(std::move(tab_name_)), name(std::move(name_)), type(type_), len(len_), offset(offset_),
          index(index_) {}

    friend std::ostream &operator<<(std::ostream &os, const ColMeta &col) {
        return os << col.tab_name << ' ' << col.name << ' ' << col.type << ' ' << col.len << ' ' << col.offset << ' '
                  << col.index;
    }

    friend std::istream &operator>>(std::istream &is, ColMeta &col) {
        return is >> col.tab_name >> col.name >> col.type >> col.len >> col.offset >> col.index;
    }
};

struct TabMeta {
    std::string name;
    std::vector<ColMeta> cols;

    bool is_col(const std::string &col_name) const {
        auto pos = std::find_if(cols.begin(), cols.end(), [&](const ColMeta &col) { return col.name == col_name; });
        return pos != cols.end();
    }

    std::vector<ColMeta>::iterator get_col(const std::string &col_name) {
        auto pos = std::find_if(cols.begin(), cols.end(), [&](const ColMeta &col) { return col.name == col_name; });
        if (pos == cols.end()) {
            throw ColumnNotFoundError(col_name);
        }
        return pos;
    }

    friend std::ostream &operator<<(std::ostream &os, const TabMeta &tab) {
        os << tab.name << '\n' << tab.cols.size() << '\n';
        for (auto &col : tab.cols) {
            os << col << '\n';
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, TabMeta &tab) {
        size_t n;
        is >> tab.name >> n;
        for (size_t i = 0; i < n; i++) {
            ColMeta col;
            is >> col;
            tab.cols.push_back(col);
        }
        return is;
    }
};

struct DbMeta {
    std::string name;
    std::map<std::string, TabMeta> tabs;

    bool is_table(const std::string &tab_name) const { return tabs.find(tab_name) != tabs.end(); }

    TabMeta &get_table(const std::string &tab_name) {
        auto pos = tabs.find(tab_name);
        if (pos == tabs.end()) {
            throw TableNotFoundError(tab_name);
        }
        return pos->second;
    }

    friend std::ostream &operator<<(std::ostream &os, const DbMeta &db_meta) {
        os << db_meta.name << '\n' << db_meta.tabs.size() << '\n';
        for (auto &entry : db_meta.tabs) {
            os << entry.second << '\n';
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, DbMeta &db_meta) {
        size_t n;
        is >> db_meta.name >> n;
        for (size_t i = 0; i < n; i++) {
            TabMeta tab;
            is >> tab;
            db_meta.tabs[tab.name] = tab;
        }
        return is;
    }
};
