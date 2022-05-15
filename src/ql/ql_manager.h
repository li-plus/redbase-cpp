#pragma once

#include "ql/ql_defs.h"
#include "rm/rm.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

struct TabCol {
    std::string tab_name;
    std::string col_name;

    TabCol() = default;
    TabCol(std::string tab_name_, std::string col_name_)
        : tab_name(std::move(tab_name_)), col_name(std::move(col_name_)) {}

    friend bool operator<(const TabCol &x, const TabCol &y) {
        return std::make_pair(x.tab_name, x.col_name) < std::make_pair(y.tab_name, y.col_name);
    }
};

struct Value {
    ColType type; // type of value
    union {
        int int_val;     // int value
        float float_val; // float value
    };
    std::string str_val; // string value

    std::shared_ptr<RmRecord> raw; // raw record buffer

    void set_int(int int_val_) {
        type = TYPE_INT;
        int_val = int_val_;
    }

    void set_float(float float_val_) {
        type = TYPE_FLOAT;
        float_val = float_val_;
    }

    void set_str(std::string str_val_) {
        type = TYPE_STRING;
        str_val = std::move(str_val_);
    }

    void init_raw(int len) {
        assert(raw == nullptr);
        raw = std::make_shared<RmRecord>(len);
        if (type == TYPE_INT) {
            assert(len == sizeof(int));
            *(int *)(raw->data) = int_val;
        } else if (type == TYPE_FLOAT) {
            assert(len == sizeof(float));
            *(float *)(raw->data) = float_val;
        } else if (type == TYPE_STRING) {
            if (len < (int)str_val.size()) {
                throw StringOverflowError();
            }
            memset(raw->data, 0, len);
            memcpy(raw->data, str_val.c_str(), str_val.size());
        }
    }
};

enum CompOp { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };

struct Condition {
    TabCol lhs_col;  // left-hand side column
    CompOp op;       // comparison operator
    bool is_rhs_val; // true if right-hand side is a value (not a column)
    TabCol rhs_col;  // right-hand side column
    Value rhs_val;   // right-hand side value
};

struct SetClause {
    TabCol lhs;
    Value rhs;

    SetClause() = default;
    SetClause(TabCol lhs_, Value rhs_) : lhs(std::move(lhs_)), rhs(std::move(rhs_)) {}
};

class QlManager {
  public:
    static void insert_into(const std::string &tab_name, std::vector<Value> values);

    static void delete_from(const std::string &tab_name, std::vector<Condition> conds);

    static void update_set(const std::string &tab_name, std::vector<SetClause> set_clauses,
                           std::vector<Condition> conds);

    static void select_from(std::vector<TabCol> sel_cols, const std::vector<std::string> &tab_names,
                            std::vector<Condition> conds);
};
