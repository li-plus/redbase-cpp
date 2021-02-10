#include "interp.h"
#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: redbase <database>" << std::endl;
        exit(1);
    }
    try {
        std::cout << "\n"
                     "  ██████╗ ███████╗██████╗ ██████╗  █████╗ ███████╗███████╗\n"
                     "  ██╔══██╗██╔════╝██╔══██╗██╔══██╗██╔══██╗██╔════╝██╔════╝\n"
                     "  ██████╔╝█████╗  ██║  ██║██████╔╝███████║███████╗█████╗  \n"
                     "  ██╔══██╗██╔══╝  ██║  ██║██╔══██╗██╔══██║╚════██║██╔══╝  \n"
                     "  ██║  ██║███████╗██████╔╝██████╔╝██║  ██║███████║███████╗\n"
                     "  ╚═╝  ╚═╝╚══════╝╚═════╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝\n"
                     "\n"
                     "Type 'help;' for help.\n"
                     "\n";
        // Database name is passed by args
        std::string db_name = argv[1];
        if (!SmManager::is_dir(db_name)) {
            // Database not found, create a new one
            SmManager::create_db(db_name);
        }
        // Open database
        SmManager::open_db(db_name);

        // Wait for user input
        bool is_cont = true;  // continue?
        while (is_cont) {
            char *line = readline("redbase> ");
            if (line == nullptr) {
                // EOF encountered
                break;
            }
            if (strlen(line) > 0) {
                add_history(line);
                YY_BUFFER_STATE buf = yy_scan_string(line);
                if (yyparse() == 0) {
                    if (ast::parse_tree != nullptr) {
                        try {
                            Interp::interp_sql(ast::parse_tree);
                        } catch (RedBaseError &e) {
                            std::cerr << e.what() << std::endl;
                        }
                    } else {
                        is_cont = false;
                    }
                }
                yy_delete_buffer(buf);
            }
            free(line);
        }
        SmManager::close_db();
        std::cout << "Bye" << std::endl;
    } catch (RedBaseError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
