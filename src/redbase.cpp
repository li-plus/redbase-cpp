#include "interp.h"
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>

static bool should_exit = false;

void sigint_handler(int signo) { should_exit = true; }

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <database>" << std::endl;
        exit(1);
    }
    signal(SIGINT, sigint_handler);
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
        while (!should_exit) {
            char *line_read = readline("redbase> ");
            if (line_read == nullptr) {
                // EOF encountered
                break;
            }
            std::string line = line_read;
            free(line_read);

            if (should_exit) {
                break;
            }

            if (!line.empty()) {
                add_history(line.c_str());
                YY_BUFFER_STATE buf = yy_scan_string(line.c_str());
                if (yyparse() == 0) {
                    if (ast::parse_tree != nullptr) {
                        try {
                            Interp::interp_sql(ast::parse_tree);
                        } catch (RedBaseError &e) {
                            std::cerr << e.what() << std::endl;
                        }
                    } else {
                        should_exit = true;
                    }
                }
                yy_delete_buffer(buf);
            }
        }
        SmManager::close_db();
        std::cout << "Bye" << std::endl;
    } catch (RedBaseError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
