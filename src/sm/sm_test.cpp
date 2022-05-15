#include "sm/sm.h"
#include <gtest/gtest.h>

TEST(sm, basic) {
    std::string db = "db";
    std::string tab1 = "tab1";
    std::string tab2 = "tab2";
    if (SmManager::is_dir(db)) {
        SmManager::drop_db(db);
    }
    // Cannot use a database that does not exist
    EXPECT_THROW(SmManager::open_db(db), DatabaseNotFoundError);

    // Create database
    SmManager::create_db(db);
    // Cannot re-create database
    EXPECT_THROW(SmManager::create_db(db), DatabaseExistsError);

    // Open database
    SmManager::open_db(db);
    std::vector<ColDef> col_defs = {ColDef("a", TYPE_INT, 4), ColDef("b", TYPE_FLOAT, 4),
                                    ColDef("c", TYPE_STRING, 256)};
    // Create table 1
    SmManager::create_table(tab1, col_defs);
    // Cannot re-create table
    EXPECT_THROW(SmManager::create_table(tab1, col_defs), TableExistsError);

    // Create table 2
    SmManager::create_table(tab2, col_defs);
    // Create index for table 1
    SmManager::create_index(tab1, "a");
    SmManager::create_index(tab1, "c");
    // Cannot re-create index
    EXPECT_THROW(SmManager::create_index(tab1, "a"), IndexExistsError);

    // Create index for table 2
    SmManager::create_index(tab2, "b");
    // Drop index of table 1
    SmManager::drop_index(tab1, "a");
    // Cannot drop index that does not exist
    EXPECT_THROW(SmManager::drop_index(tab1, "b"), IndexNotFoundError);

    // Drop index
    SmManager::drop_table(tab1);
    // Cannot drop table that does not exist
    EXPECT_THROW(SmManager::drop_table(tab1), TableNotFoundError);

    // Clean up
    SmManager::close_db();
    SmManager::drop_db(db);
}