#include "ql/ql_node.h"

std::vector<ColMeta>::const_iterator QlNode::get_col(const std::vector<ColMeta> &rec_cols, const TabCol &target) {
    auto pos = std::find_if(rec_cols.begin(), rec_cols.end(), [&](const ColMeta &col) {
        return col.tab_name == target.tab_name && col.name == target.col_name;
    });
    if (pos == rec_cols.end()) {
        throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
    }
    return pos;
}

std::map<TabCol, Value> QlNode::rec2dict(const std::vector<ColMeta> &cols, const RmRecord *rec) {
    std::map<TabCol, Value> rec_dict;
    for (auto &col : cols) {
        TabCol key(col.tab_name, col.name);
        Value val;
        uint8_t *val_buf = rec->data + col.offset;
        if (col.type == TYPE_INT) {
            val.set_int(*(int *)val_buf);
        } else if (col.type == TYPE_FLOAT) {
            val.set_float(*(float *)val_buf);
        } else if (col.type == TYPE_STRING) {
            std::string str_val((char *)val_buf, col.len);
            str_val.resize(strlen(str_val.c_str()));
            val.set_str(str_val);
        }
        assert(rec_dict.count(key) == 0);
        val.init_raw(col.len);
        rec_dict[key] = val;
    }
    return rec_dict;
}

QlNodeProj::QlNodeProj(std::unique_ptr<QlNode> prev, const std::vector<TabCol> &sel_cols) {
    _prev = std::move(prev);

    size_t curr_offset = 0;
    auto &prev_cols = _prev->cols();
    for (auto &sel_col : sel_cols) {
        auto pos = get_col(prev_cols, sel_col);
        _sel_idxs.push_back(pos - prev_cols.begin());
        auto col = *pos;
        col.offset = curr_offset;
        curr_offset += col.len;
        _cols.push_back(col);
    }
    _len = curr_offset;
}

std::unique_ptr<RmRecord> QlNodeProj::rec() const {
    assert(!is_end());
    auto &prev_cols = _prev->cols();
    auto prev_rec = _prev->rec();
    auto &proj_cols = _cols;
    auto proj_rec = std::make_unique<RmRecord>(_len);
    for (size_t proj_idx = 0; proj_idx < proj_cols.size(); proj_idx++) {
        size_t prev_idx = _sel_idxs[proj_idx];
        auto &prev_col = prev_cols[prev_idx];
        auto &proj_col = proj_cols[proj_idx];
        memcpy(proj_rec->data + proj_col.offset, prev_rec->data + prev_col.offset, proj_col.len);
    }
    return proj_rec;
}

QlNodeTable::QlNodeTable(std::string tab_name, std::vector<Condition> conds) {
    _tab_name = std::move(tab_name);
    _conds = std::move(conds);
    TabMeta &tab = SmManager::db.get_table(_tab_name);
    _fh = SmManager::fhs.at(_tab_name).get();
    _cols = tab.cols;
    _len = _cols.back().offset + _cols.back().len;
    static std::map<CompOp, CompOp> swap_op = {
        {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
    };

    for (auto &cond : _conds) {
        if (cond.lhs_col.tab_name != _tab_name) {
            // lhs is on other table, now rhs must be on this table
            assert(!cond.is_rhs_val && cond.rhs_col.tab_name == _tab_name);
            // swap lhs and rhs
            std::swap(cond.lhs_col, cond.rhs_col);
            cond.op = swap_op.at(cond.op);
        }
    }
    _fed_conds = _conds;
}

void QlNodeTable::begin() {
    check_runtime_conds();

    int index_no = -1;

    TabMeta &tab = SmManager::db.get_table(_tab_name);
    for (auto &cond : _fed_conds) {
        if (cond.is_rhs_val && cond.op != OP_NE) {
            // If rhs is value and op is not "!=", find if lhs has index
            auto lhs_col = tab.get_col(cond.lhs_col.col_name);
            if (lhs_col->index) {
                // This column has index, use it
                index_no = lhs_col - tab.cols.begin();
                break;
            }
        }
    }

    if (index_no == -1) {
        // no index is available, scan record file
        _scan = std::make_unique<RmScan>(_fh);
    } else {
        // index is available, scan index
        auto ih = SmManager::ihs.at(IxManager::get_index_name(_tab_name, index_no)).get();
        Iid lower = ih->leaf_begin();
        Iid upper = ih->leaf_end();
        auto &index_col = _cols[index_no];
        for (auto &cond : _fed_conds) {
            if (cond.is_rhs_val && cond.op != OP_NE && cond.lhs_col.col_name == index_col.name) {
                uint8_t *rhs_key = cond.rhs_val.raw->data;
                if (cond.op == OP_EQ) {
                    lower = ih->lower_bound(rhs_key);
                    upper = ih->upper_bound(rhs_key);
                } else if (cond.op == OP_LT) {
                    upper = ih->lower_bound(rhs_key);
                } else if (cond.op == OP_GT) {
                    lower = ih->upper_bound(rhs_key);
                } else if (cond.op == OP_LE) {
                    upper = ih->upper_bound(rhs_key);
                } else if (cond.op == OP_GE) {
                    lower = ih->lower_bound(rhs_key);
                } else {
                    throw InternalError("Unexpected op type");
                }
                break; // TODO: maintain an interval
            }
        }
        _scan = std::make_unique<IxScan>(ih, lower, upper);
    }
    // Get the first record
    while (!_scan->is_end()) {
        _rid = _scan->rid();
        auto rec = _fh->get_record(_rid);
        if (eval_conds(_cols, _fed_conds, rec.get())) {
            break;
        }
        _scan->next();
    }
}

void QlNodeTable::next() {
    check_runtime_conds();
    assert(!is_end());
    for (_scan->next(); !_scan->is_end(); _scan->next()) {
        _rid = _scan->rid();
        auto rec = _fh->get_record(_rid);
        if (eval_conds(_cols, _fed_conds, rec.get())) {
            break;
        }
    }
}

bool QlNodeTable::is_end() const { return _scan->is_end(); }

void QlNodeTable::feed(const std::map<TabCol, Value> &feed_dict) {
    _fed_conds = _conds;
    for (auto &cond : _fed_conds) {
        if (!cond.is_rhs_val && cond.rhs_col.tab_name != _tab_name) {
            // need to feed rhs col
            cond.is_rhs_val = true;
            cond.rhs_val = feed_dict.at(cond.rhs_col);
        }
    }
    check_runtime_conds();
}

void QlNodeTable::check_runtime_conds() {
    for (auto &cond : _fed_conds) {
        assert(cond.lhs_col.tab_name == _tab_name);
        if (!cond.is_rhs_val) {
            assert(cond.rhs_col.tab_name == _tab_name);
        }
    }
}

bool QlNodeTable::eval_cond(const std::vector<ColMeta> &rec_cols, const Condition &cond, const RmRecord *rec) {
    auto lhs_col = get_col(rec_cols, cond.lhs_col);
    uint8_t *lhs = rec->data + lhs_col->offset;
    uint8_t *rhs;
    ColType rhs_type;
    if (cond.is_rhs_val) {
        rhs_type = cond.rhs_val.type;
        rhs = cond.rhs_val.raw->data;
    } else {
        // rhs is a column
        auto rhs_col = get_col(rec_cols, cond.rhs_col);
        rhs_type = rhs_col->type;
        rhs = rec->data + rhs_col->offset;
    }
    assert(rhs_type == lhs_col->type); // TODO convert to common type
    int cmp = ix_compare(lhs, rhs, rhs_type, lhs_col->len);
    if (cond.op == OP_EQ) {
        return cmp == 0;
    } else if (cond.op == OP_NE) {
        return cmp != 0;
    } else if (cond.op == OP_LT) {
        return cmp < 0;
    } else if (cond.op == OP_GT) {
        return cmp > 0;
    } else if (cond.op == OP_LE) {
        return cmp <= 0;
    } else if (cond.op == OP_GE) {
        return cmp >= 0;
    } else {
        throw InternalError("Unexpected op type");
    }
}

bool QlNodeTable::eval_conds(const std::vector<ColMeta> &rec_cols, const std::vector<Condition> &conds,
                             const RmRecord *rec) {
    return std::all_of(conds.begin(), conds.end(),
                       [&](const Condition &cond) { return eval_cond(rec_cols, cond, rec); });
}

QlNodeJoin::QlNodeJoin(std::unique_ptr<QlNode> left, std::unique_ptr<QlNode> right) {
    _left = std::move(left);
    _right = std::move(right);
    _len = _left->len() + _right->len();
    _cols = _left->cols();
    auto right_cols = _right->cols();
    for (auto &col : right_cols) {
        col.offset += _left->len();
    }
    _cols.insert(_cols.end(), right_cols.begin(), right_cols.end());
}

void QlNodeJoin::begin() {
    _left->begin();
    if (_left->is_end()) {
        return;
    }
    feed_right();
    _right->begin();
    while (_right->is_end()) {
        _left->next();
        if (_left->is_end()) {
            break;
        }
        feed_right();
        _right->begin();
    }
}

void QlNodeJoin::next() {
    assert(!is_end());
    _right->next();
    while (_right->is_end()) {
        _left->next();
        if (_left->is_end()) {
            break;
        }
        feed_right();
        _right->begin();
    }
}

std::unique_ptr<RmRecord> QlNodeJoin::rec() const {
    assert(!is_end());
    auto record = std::make_unique<RmRecord>(_len);
    memcpy(record->data, _left->rec()->data, _left->len());
    memcpy(record->data + _left->len(), _right->rec()->data, _right->len());
    return record;
}

void QlNodeJoin::feed(const std::map<TabCol, Value> &feed_dict) {
    _prev_feed_dict = feed_dict;
    _left->feed(feed_dict);
}

void QlNodeJoin::feed_right() {
    auto left_dict = rec2dict(_left->cols(), _left->rec().get());
    auto feed_dict = _prev_feed_dict;
    feed_dict.insert(left_dict.begin(), left_dict.end());
    _right->feed(feed_dict);
}
