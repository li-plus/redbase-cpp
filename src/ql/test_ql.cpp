#include "ql.h"
#include "interp.h"

void exec_sql(const std::string &sql) {
    std::cout << sql << std::endl;
    YY_BUFFER_STATE buffer = yy_scan_string(sql.c_str());
    assert(yyparse() == 0 && ast::parse_tree != nullptr);
    yy_delete_buffer(buffer);
    Interp::interp_sql(ast::parse_tree);
}

int main() {
    std::string db_name = "db";
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

    try {
        exec_sql("insert into tb1 values (0, 1, 2., 100);");
        assert(0);
    } catch (IncompatibleTypeError &) {}
    try {
        exec_sql("insert into tb1 values (0, 1, 2., 'abc', 1);");
        assert(0);
    } catch (InvalidValueCountError &) {}
    try {
        exec_sql("insert into oops values (1, 2);");
        assert(0);
    } catch (TableNotFoundError &) {}
    try {
        exec_sql("create table tb1 (a int, b float);");
        assert(0);
    } catch (TableExistsError &) {}
    try {
        exec_sql("create index tb1(oops);");
        assert(0);
    } catch (ColumnNotFoundError &e) {}
    try {
        exec_sql("create index tb1(s);");
        assert(0);
    } catch (IndexExistsError &e) {}
    try {
        exec_sql("drop index tb1(a);");
        assert(0);
    } catch (IndexNotFoundError &) {}
    try {
        exec_sql("insert into tb1 values (0, 1, 2., '0123456789abcdefg');");
        assert(0);
    } catch (StringOverflowError &) {}
    try {
        exec_sql("select x from tb1;");
        assert(0);
    } catch (ColumnNotFoundError &) {}
    try {
        exec_sql("select s from tb1, tb2;");
        assert(0);
    } catch (AmbiguousColumnError &) {}
    try {
        exec_sql("select * from tb1, tb2 where s = 1;");
        assert(0);
    } catch (AmbiguousColumnError &) {}
    try {
        exec_sql("select * from tb1 where s = 'oops';");
        assert(0);
    } catch (IncompatibleTypeError &) {}
    exec_sql("select * from tb1;");
    exec_sql("select * from tb2;");
    exec_sql("select * from tb1, tb2;");
    SmManager::close_db();
    return 0;
}
