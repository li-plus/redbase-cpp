#include "interp.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <database>" << std::endl;
        exit(1);
    }
    try {
        std::string db_name = argv[1];
        if (!SmManager::is_dir(db_name)) {
            SmManager::create_db(db_name);
        }
        SmManager::open_db(db_name);

        while (true) {
            if (yyparse() == 0) {
                if (ast::parse_tree == nullptr) {
                    break;
                }
                try {
                    Interp::interp_sql(ast::parse_tree);
                } catch (RedBaseError &e) {
                    std::cerr << e.what() << std::endl;
                }
            }
        }
        SmManager::close_db();
    } catch (RedBaseError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
