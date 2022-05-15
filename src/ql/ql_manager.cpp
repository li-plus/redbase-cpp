#include "ql/ql_manager.h"
#include "ix/ix.h"
#include "ql/ql_node.h"
#include "record_printer.h"
#include "sm/sm.h"

static TabCol check_column(const std::vector<ColMeta> &all_cols, TabCol target) {
    if (target.tab_name.empty()) {
        // Table name not specified, infer table name from column name
        std::string tab_name;
        for (auto &col : all_cols) {
            if (col.name == target.col_name) {
                if (!tab_name.empty()) {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty()) {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = tab_name;
    } else {
        // Make sure target column exists
        if (!(SmManager::db.is_table(target.tab_name) &&
              SmManager::db.get_table(target.tab_name).is_col(target.col_name))) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
    }
    return target;
}

static std::vector<ColMeta> get_all_cols(const std::vector<std::string> &tab_names) {
    std::vector<ColMeta> all_cols;
    for (auto &sel_tab_name : tab_names) {
        const auto &sel_tab_cols = SmManager::db.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
    return all_cols;
}

static std::vector<Condition> check_where_clause(const std::vector<std::string> &tab_names,
                                                 const std::vector<Condition> &conds) {
    auto all_cols = get_all_cols(tab_names);
    // Get raw values in where clause
    std::vector<Condition> res_conds = conds;
    for (auto &cond : res_conds) {
        // Infer table name from column name
        cond.lhs_col = check_column(all_cols, cond.lhs_col);
        if (!cond.is_rhs_val) {
            cond.rhs_col = check_column(all_cols, cond.rhs_col);
        }
        TabMeta &lhs_tab = SmManager::db.get_table(cond.lhs_col.tab_name);
        auto lhs_col = lhs_tab.get_col(cond.lhs_col.col_name);
        ColType lhs_type = lhs_col->type;
        ColType rhs_type;
        if (cond.is_rhs_val) {
            cond.rhs_val.init_raw(lhs_col->len);
            rhs_type = cond.rhs_val.type;
        } else {
            TabMeta &rhs_tab = SmManager::db.get_table(cond.rhs_col.tab_name);
            auto rhs_col = rhs_tab.get_col(cond.rhs_col.col_name);
            rhs_type = rhs_col->type;
        }
        if (lhs_type != rhs_type) {
            throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(rhs_type));
        }
    }
    return res_conds;
}

void QlManager::insert_into(const std::string &tab_name, std::vector<Value> values) {
    TabMeta tab = SmManager::db.get_table(tab_name);
    if (values.size() != tab.cols.size()) {
        throw InvalidValueCountError();
    }
    // Get record file handle
    auto fh = SmManager::fhs.at(tab_name).get();
    // Make record buffer
    RmRecord rec(fh->hdr.record_size);
    for (size_t i = 0; i < values.size(); i++) {
        auto &col = tab.cols[i];
        auto &val = values[i];
        if (col.type != val.type) {
            throw IncompatibleTypeError(coltype2str(col.type), coltype2str(val.type));
        }
        val.init_raw(col.len);
        memcpy(rec.data + col.offset, val.raw->data, col.len);
    }
    // Insert into record file
    Rid rid = fh->insert_record(rec.data);
    // Insert into index
    for (size_t i = 0; i < tab.cols.size(); i++) {
        auto &col = tab.cols[i];
        if (col.index) {
            auto ih = SmManager::ihs.at(IxManager::get_index_name(tab_name, i)).get();
            ih->insert_entry(rec.data + col.offset, rid);
        }
    }
}

void QlManager::delete_from(const std::string &tab_name, std::vector<Condition> conds) {
    TabMeta &tab = SmManager::db.get_table(tab_name);
    // Parse where clause
    conds = check_where_clause({tab_name}, conds);
    // Get all RID to delete
    std::vector<Rid> rids;
    QlNodeTable table_scan(tab_name, conds);
    for (table_scan.begin(); !table_scan.is_end(); table_scan.next()) {
        rids.push_back(table_scan.rid());
    }
    // Get record file
    auto fh = SmManager::fhs.at(tab_name).get();
    // Get all index files
    std::vector<IxIndexHandle *> ihs(tab.cols.size(), nullptr);
    for (size_t col_i = 0; col_i < tab.cols.size(); col_i++) {
        if (tab.cols[col_i].index) {
            ihs[col_i] = SmManager::ihs.at(IxManager::get_index_name(tab_name, col_i)).get();
        }
    }
    // Delete each rid from record file and index file
    for (auto &rid : rids) {
        auto rec = fh->get_record(rid);
        // Delete from index file
        for (size_t col_i = 0; col_i < tab.cols.size(); col_i++) {
            if (ihs[col_i] != nullptr) {
                ihs[col_i]->delete_entry(rec->data + tab.cols[col_i].offset, rid);
            }
        }
        // Delete from record file
        fh->delete_record(rid);
    }
}

void QlManager::update_set(const std::string &tab_name, std::vector<SetClause> set_clauses,
                           std::vector<Condition> conds) {
    TabMeta &tab = SmManager::db.get_table(tab_name);
    // Parse where clause
    conds = check_where_clause({tab_name}, conds);
    // Get raw values in set clause
    for (auto &set_clause : set_clauses) {
        auto lhs_col = tab.get_col(set_clause.lhs.col_name);
        if (lhs_col->type != set_clause.rhs.type) {
            throw IncompatibleTypeError(coltype2str(lhs_col->type), coltype2str(set_clause.rhs.type));
        }
        set_clause.rhs.init_raw(lhs_col->len);
    }
    // Get all RID to update
    std::vector<Rid> rids;
    QlNodeTable table_scan(tab_name, conds);
    for (table_scan.begin(); !table_scan.is_end(); table_scan.next()) {
        rids.push_back(table_scan.rid());
    }
    // Get record file
    auto fh = SmManager::fhs.at(tab_name).get();
    // Get all necessary index files
    std::vector<IxIndexHandle *> ihs(tab.cols.size(), nullptr);
    for (auto &set_clause : set_clauses) {
        auto lhs_col = tab.get_col(set_clause.lhs.col_name);
        if (lhs_col->index) {
            size_t lhs_col_idx = lhs_col - tab.cols.begin();
            if (ihs[lhs_col_idx] == nullptr) {
                ihs[lhs_col_idx] = SmManager::ihs.at(IxManager::get_index_name(tab_name, lhs_col_idx)).get();
            }
        }
    }
    // Update each rid from record file and index file
    for (auto &rid : rids) {
        auto rec = fh->get_record(rid);
        // Remove old entry from index
        for (size_t i = 0; i < tab.cols.size(); i++) {
            if (ihs[i] != nullptr) {
                ihs[i]->delete_entry(rec->data + tab.cols[i].offset, rid);
            }
        }
        // Update record in record file
        for (auto &set_clause : set_clauses) {
            auto lhs_col = tab.get_col(set_clause.lhs.col_name);
            memcpy(rec->data + lhs_col->offset, set_clause.rhs.raw->data, lhs_col->len);
        }
        fh->update_record(rid, rec->data);
        // Insert new entry into index
        for (size_t i = 0; i < tab.cols.size(); i++) {
            if (ihs[i] != nullptr) {
                ihs[i]->insert_entry(rec->data + tab.cols[i].offset, rid);
            }
        }
    }
}

static std::vector<Condition> pop_conds(std::vector<Condition> &conds, const std::vector<std::string> &tab_names) {
    auto has_tab = [&](const std::string &tab_name) {
        return std::find(tab_names.begin(), tab_names.end(), tab_name) != tab_names.end();
    };
    std::vector<Condition> solved_conds;
    auto it = conds.begin();
    while (it != conds.end()) {
        if (has_tab(it->lhs_col.tab_name) && (it->is_rhs_val || has_tab(it->rhs_col.tab_name))) {
            solved_conds.emplace_back(std::move(*it));
            it = conds.erase(it);
        } else {
            it++;
        }
    }
    return solved_conds;
}

void QlManager::select_from(std::vector<TabCol> sel_cols, const std::vector<std::string> &tab_names,
                            std::vector<Condition> conds) {
    // Parse selector
    auto all_cols = get_all_cols(tab_names);
    if (sel_cols.empty()) {
        // select all columns
        for (auto &col : all_cols) {
            TabCol sel_col(col.tab_name, col.name);
            sel_cols.push_back(sel_col);
        }
    } else {
        // infer table name from column name
        for (auto &sel_col : sel_cols) {
            sel_col = check_column(all_cols, sel_col);
        }
    }
    // Parse where clause
    conds = check_where_clause(tab_names, conds);
    // Scan table
    std::vector<std::unique_ptr<QlNodeTable>> tab_nodes(tab_names.size());
    for (size_t i = 0; i < tab_names.size(); i++) {
        auto curr_conds = pop_conds(conds, {tab_names.begin(), tab_names.begin() + i + 1});
        tab_nodes[i] = std::make_unique<QlNodeTable>(tab_names[i], curr_conds);
    }
    assert(conds.empty());
    std::unique_ptr<QlNode> query_plan = std::move(tab_nodes.back());
    for (size_t i = tab_names.size() - 2; i != (size_t)-1; i--) {
        query_plan = std::make_unique<QlNodeJoin>(std::move(tab_nodes[i]), std::move(query_plan));
    }
    query_plan = std::make_unique<QlNodeProj>(std::move(query_plan), sel_cols);
    // Column titles
    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (auto &sel_col : sel_cols) {
        captions.push_back(sel_col.col_name);
    }
    // Print header
    RecordPrinter rec_printer(sel_cols.size());
    rec_printer.print_separator();
    rec_printer.print_record(captions);
    rec_printer.print_separator();
    // Print records
    size_t num_rec = 0;
    for (query_plan->begin(); !query_plan->is_end(); query_plan->next()) {
        auto rec = query_plan->rec();
        std::vector<std::string> columns;
        for (auto &col : query_plan->cols()) {
            std::string col_str;
            uint8_t *rec_buf = rec->data + col.offset;
            if (col.type == TYPE_INT) {
                col_str = std::to_string(*(int *)rec_buf);
            } else if (col.type == TYPE_FLOAT) {
                col_str = std::to_string(*(float *)rec_buf);
            } else if (col.type == TYPE_STRING) {
                col_str = std::string((char *)rec_buf, col.len);
                col_str.resize(strlen(col_str.c_str()));
            }
            columns.push_back(col_str);
        }
        rec_printer.print_record(columns);
        num_rec++;
    }
    // Print footer
    rec_printer.print_separator();
    // Print record count
    RecordPrinter::print_record_count(num_rec);
}
