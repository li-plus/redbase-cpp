#undef NDEBUG

#include "sm.h"
#include <string>
#include <cassert>

int main() {
    std::string db = "db";
    std::string tab1 = "tab1";
    std::string tab2 = "tab2";
    if (SmManager::is_dir(db)) {
        SmManager::drop_db(db);
    }
    // Cannot use a database that does not exist
    try {
        SmManager::open_db(db);
        assert(0);
    } catch (DatabaseNotFoundError &) {}
    // Create database
    SmManager::create_db(db);
    // Cannot re-create database
    try {
        SmManager::create_db(db);
        assert(0);
    } catch (DatabaseExistsError &) {}
    // Open database
    SmManager::open_db(db);
    std::vector<ColDef> col_defs = {
            {.name = "a", .type = TYPE_INT, .len = 4},
            {.name = "b", .type = TYPE_FLOAT, .len=4},
            {.name = "c", .type = TYPE_STRING, .len=256}
    };
    // Create table 1
    SmManager::create_table(tab1, col_defs);
    // Cannot re-create table
    try {
        SmManager::create_table(tab1, col_defs);
        assert(0);
    } catch (TableExistsError &) {}
    // Create table 2
    SmManager::create_table(tab2, col_defs);
    // Create index for table 1
    SmManager::create_index(tab1, "a");
    SmManager::create_index(tab1, "c");
    // Cannot re-create index
    try {
        SmManager::create_index(tab1, "a");
        assert(0);
    } catch (IndexExistsError &) {}
    // Create index for table 2
    SmManager::create_index(tab2, "b");
    // Drop index of table 1
    SmManager::drop_index(tab1, "a");
    // Cannot drop index that does not exist
    try {
        SmManager::drop_index(tab1, "b");
        assert(0);
    } catch (IndexNotFoundError &) {}
    // Drop index
    SmManager::drop_table(tab1);
    // Cannot drop table that does not exist
    try {
        SmManager::drop_table(tab1);
        assert(0);
    } catch (TableNotFoundError &) {}
    // Clean up
    SmManager::close_db();
    SmManager::drop_db(db);
    return 0;
}