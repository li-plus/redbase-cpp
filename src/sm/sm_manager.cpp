#include "sm/sm_manager.h"
#include "ix/ix.h"
#include "record_printer.h"
#include "rm/rm.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

DbMeta SmManager::db;
std::map<std::string, std::unique_ptr<RmFileHandle>> SmManager::fhs;
std::map<std::string, std::unique_ptr<IxIndexHandle>> SmManager::ihs;

bool SmManager::is_dir(const std::string &db_name) {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void SmManager::create_db(const std::string &db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    // Create a subdirectory for the database
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    // Create the system catalogs
    DbMeta new_db;
    new_db.name = db_name;
    std::ofstream ofs(DB_META_NAME);
    ofs << new_db;
    // cd back to root dir
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

void SmManager::drop_db(const std::string &db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

void SmManager::open_db(const std::string &db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    // cd to database dir
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    // Load meta
    std::ifstream ifs(DB_META_NAME);
    ifs >> db;
    // Open all record files & index files
    for (auto &entry : db.tabs) {
        auto &tab = entry.second;
        fhs[tab.name] = RmManager::open_file(tab.name);
        for (size_t i = 0; i < tab.cols.size(); i++) {
            auto &col = tab.cols[i];
            if (col.index) {
                auto index_name = IxManager::get_index_name(tab.name, i);
                assert(ihs.count(index_name) == 0);
                ihs[index_name] = IxManager::open_index(tab.name, i);
            }
        }
    }
}

void SmManager::close_db() {
    // Dump meta
    std::ofstream ofs(DB_META_NAME);
    ofs << db;
    db.name.clear();
    db.tabs.clear();
    // Close all record files
    for (auto &entry : fhs) {
        RmManager::close_file(entry.second.get());
    }
    fhs.clear();
    // Close all index files
    for (auto &entry : ihs) {
        IxManager::close_index(entry.second.get());
    }
    ihs.clear();
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

void SmManager::show_tables() {
    RecordPrinter printer(1);
    printer.print_separator();
    printer.print_record({"Tables"});
    printer.print_separator();
    for (auto &entry : db.tabs) {
        auto &tab = entry.second;
        printer.print_record({tab.name});
    }
    printer.print_separator();
}

void SmManager::desc_table(const std::string &tab_name) {
    TabMeta &tab = db.get_table(tab_name);

    std::vector<std::string> captions = {"Field", "Type", "Index"};
    RecordPrinter printer(captions.size());
    // Print header
    printer.print_separator();
    printer.print_record(captions);
    printer.print_separator();
    // Print fields
    for (auto &col : tab.cols) {
        std::vector<std::string> field_info = {col.name, coltype2str(col.type), col.index ? "YES" : "NO"};
        printer.print_record(field_info);
    }
    // Print footer
    printer.print_separator();
}

void SmManager::create_table(const std::string &tab_name, const std::vector<ColDef> &col_defs) {
    if (db.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
    // Create table meta
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    for (auto &col_def : col_defs) {
        ColMeta col(tab_name, col_def.name, col_def.type, col_def.len, curr_offset, false);
        curr_offset += col_def.len;
        tab.cols.push_back(col);
    }
    // Create & open record file
    int record_size = curr_offset;
    RmManager::create_file(tab_name, record_size);
    db.tabs[tab_name] = tab;
    fhs[tab_name] = RmManager::open_file(tab_name);
}

void SmManager::drop_table(const std::string &tab_name) {
    // Find table index in db meta
    TabMeta &tab = db.get_table(tab_name);
    // Close & destroy record file
    RmManager::close_file(fhs.at(tab_name).get());
    RmManager::destroy_file(tab_name);
    // Close & destroy index file
    for (auto &col : tab.cols) {
        if (col.index) {
            SmManager::drop_index(tab_name, col.name);
        }
    }
    db.tabs.erase(tab_name);
    fhs.erase(tab_name);
}

void SmManager::create_index(const std::string &tab_name, const std::string &col_name) {
    TabMeta &tab = db.get_table(tab_name);
    auto col = tab.get_col(col_name);
    if (col->index) {
        throw IndexExistsError(tab_name, col_name);
    }
    // Create index file
    int col_idx = col - tab.cols.begin();
    IxManager::create_index(tab_name, col_idx, col->type, col->len);
    // Open index file
    auto ih = IxManager::open_index(tab_name, col_idx);
    // Get record file handle
    auto fh = fhs.at(tab_name).get();
    // Index all records into index
    for (RmScan rm_scan(fh); !rm_scan.is_end(); rm_scan.next()) {
        auto rec = fh->get_record(rm_scan.rid());
        const uint8_t *key = rec->data + col->offset;
        ih->insert_entry(key, rm_scan.rid());
    }
    // Store index handle
    auto index_name = IxManager::get_index_name(tab_name, col_idx);
    assert(ihs.count(index_name) == 0);
    ihs[index_name] = std::move(ih);
    // Mark column index as created
    col->index = true;
}

void SmManager::drop_index(const std::string &tab_name, const std::string &col_name) {
    TabMeta &tab = db.tabs[tab_name];
    auto col = tab.get_col(col_name);
    if (!col->index) {
        throw IndexNotFoundError(tab_name, col_name);
    }
    int col_idx = col - tab.cols.begin();
    auto index_name = IxManager::get_index_name(tab_name, col_idx);
    IxManager::close_index(ihs.at(index_name).get());
    IxManager::destroy_index(tab_name, col_idx);
    ihs.erase(index_name);
    col->index = false;
}
