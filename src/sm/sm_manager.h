#pragma once

#include "ix/ix.h"
#include "rm/rm.h"
#include "sm/sm_defs.h"
#include "sm/sm_meta.h"

struct ColDef {
    std::string name; // Column name
    ColType type;     // Type of column
    int len;          // Length of column

    ColDef() = default;
    ColDef(std::string name_, ColType type_, int len_) : name(std::move(name_)), type(type_), len(len_) {}
};

class SmManager {
  public:
    static DbMeta db;
    static std::map<std::string, std::unique_ptr<RmFileHandle>> fhs;
    static std::map<std::string, std::unique_ptr<IxIndexHandle>> ihs;

    // Database management
    static bool is_dir(const std::string &db_name);

    static void create_db(const std::string &db_name);

    static void drop_db(const std::string &db_name);

    static void open_db(const std::string &db_name);

    static void close_db();

    // Table management
    static void show_tables();

    static void desc_table(const std::string &tab_name);

    static void create_table(const std::string &tab_name, const std::vector<ColDef> &col_defs);

    static void drop_table(const std::string &tab_name);

    // Index management
    static void create_index(const std::string &tab_name, const std::string &col_name);

    static void drop_index(const std::string &tab_name, const std::string &col_name);
};
