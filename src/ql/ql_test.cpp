#include "interp.h"
#include "ql/ql.h"
#include <gtest/gtest.h>

void exec_sql(const std::string &sql) {
    std::cout << sql << std::endl;
    YY_BUFFER_STATE buffer = yy_scan_string(sql.c_str());
    EXPECT_EQ(yyparse(), 0);
    EXPECT_TRUE(ast::parse_tree != nullptr);
    yy_delete_buffer(buffer);
    Interp::interp_sql(ast::parse_tree);
}

TEST(ql, basic) {
    const std::string db_name = "db";
    if (SmManager::is_dir(db_name)) {
        SmManager::drop_db(db_name);
    }
    SmManager::create_db(db_name);
    SmManager::open_db(db_name);

    exec_sql("create table tb1(s int, a int, b float, c char(16));");
    exec_sql("create table tb2(x int, y float, z char(16), s int);");
    exec_sql("create index tb1(s);");

    exec_sql("show tables;");
    exec_sql("desc tb1;");

    exec_sql("select * from tb1;");
    exec_sql("insert into tb1 values (0, 1, 1.1, 'abc');");
    exec_sql("insert into tb1 values (2, 2, 2.2, 'def');");
    exec_sql("insert into tb1 values (5, 3, 2.2, 'xyz');");
    exec_sql("insert into tb1 values (4, 4, 2.2, '0123456789abcdef');");
    exec_sql("insert into tb1 values (2, 5, -1.11, '');");

    exec_sql("insert into tb2 values (1, 2., '123', 0);");
    exec_sql("insert into tb2 values (2, 3., '456', 1);");
    exec_sql("insert into tb2 values (3, 1., '789', 2);");

    exec_sql("select * from tb1, tb2;");
    exec_sql("select * from tb1, tb2 where tb1.s = tb2.s;");

    exec_sql("create index tb2(s);");
    exec_sql("select * from tb1, tb2;");
    exec_sql("select * from tb1, tb2 where tb1.s = tb2.s;");
    exec_sql("drop index tb2(s);");

    EXPECT_THROW(exec_sql("insert into tb1 values (0, 1, 2., 100);"), IncompatibleTypeError);
    EXPECT_THROW(exec_sql("insert into tb1 values (0, 1, 2., 'abc', 1);"), InvalidValueCountError);
    EXPECT_THROW(exec_sql("insert into oops values (1, 2);"), TableNotFoundError);
    EXPECT_THROW(exec_sql("create table tb1 (a int, b float);"), TableExistsError);
    EXPECT_THROW(exec_sql("create index tb1(oops);"), ColumnNotFoundError);
    EXPECT_THROW(exec_sql("create index tb1(s);"), IndexExistsError);
    EXPECT_THROW(exec_sql("drop index tb1(a);"), IndexNotFoundError);
    EXPECT_THROW(exec_sql("insert into tb1 values (0, 1, 2., '0123456789abcdefg');"), StringOverflowError);
    EXPECT_THROW(exec_sql("select x from tb1;"), ColumnNotFoundError);
    EXPECT_THROW(exec_sql("select s from tb1, tb2;"), AmbiguousColumnError);
    EXPECT_THROW(exec_sql("select * from tb1, tb2 where s = 1;"), AmbiguousColumnError);
    EXPECT_THROW(exec_sql("select * from tb1 where s = 'oops';"), IncompatibleTypeError);

    exec_sql("select * from tb1;");
    exec_sql("select * from tb2;");
    exec_sql("select * from tb1, tb2;");
    SmManager::close_db();
}
