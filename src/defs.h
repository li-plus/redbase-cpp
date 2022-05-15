#pragma once

#include <iostream>
#include <map>

template <typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
static std::ostream &operator<<(std::ostream &os, const T &enum_val) {
    return os << (int)enum_val;
}

template <typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
static std::istream &operator>>(std::istream &is, T &enum_val) {
    int int_val;
    is >> int_val;
    enum_val = static_cast<T>(int_val);
    return is;
}

struct Rid {
    int page_no;
    int slot_no;

    Rid() = default;
    Rid(int page_no_, int slot_no_) : page_no(page_no_), slot_no(slot_no_) {}

    friend bool operator==(const Rid &x, const Rid &y) { return x.page_no == y.page_no && x.slot_no == y.slot_no; }
    friend bool operator!=(const Rid &x, const Rid &y) { return !(x == y); }
};

enum ColType { TYPE_INT, TYPE_FLOAT, TYPE_STRING };

static inline std::string coltype2str(ColType type) {
    static std::map<ColType, std::string> m = {{TYPE_INT, "INT"}, {TYPE_FLOAT, "FLOAT"}, {TYPE_STRING, "STRING"}};
    return m.at(type);
}

class RecScan {
  public:
    virtual ~RecScan() = default;

    virtual void next() = 0;

    virtual bool is_end() const = 0;

    virtual Rid rid() const = 0;
};
