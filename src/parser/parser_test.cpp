#include "parser/parser.h"
#include <gtest/gtest.h>

TEST(parser, basic) {
    std::vector<std::string> sqls = {
        "show tables;",
        "desc tb;",
        "create table tb (a int, b float, c char(4));",
        "drop table tb;",
        "create index tb(a);",
        "drop index tb(b);",
        "insert into tb values (1, 3.14, 'pi');",
        "delete from tb where a = 1;",
        "update tb set a = 1, b = 2.2, c = 'xyz' where x = 2 and y < 1.1 and z > 'abc';",
        "select * from tb;",
        "select * from tb where x <> 2 and y >= 3. and z <= '123' and b < tb.a;",
        "select x.a, y.b from x, y where x.a = y.b and c = d;",
        "exit;",
        "help;",
        "",
    };
    for (auto &sql : sqls) {
        std::cout << sql << std::endl;
        YY_BUFFER_STATE buf = yy_scan_string(sql.c_str());
        EXPECT_EQ(yyparse(), 0);
        if (ast::parse_tree != nullptr) {
            ast::TreePrinter::print(ast::parse_tree);
            yy_delete_buffer(buf);
            std::cout << std::endl;
        } else {
            std::cout << "exit/EOF" << std::endl;
        }
    }
    ast::parse_tree.reset();
}
