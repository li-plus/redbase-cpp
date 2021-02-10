#pragma once

#include "ix_defs.h"
#include <memory>

class IxIndexHandle;

class IxScan : public RecScan {
    std::shared_ptr<IxIndexHandle> _ih;
    Iid _iid;
    Iid _end;
public:
    IxScan(std::shared_ptr<IxIndexHandle> ih, const Iid &lower, const Iid &upper) :
            _ih(std::move(ih)), _iid(lower), _end(upper) {}

    void next() override;

    bool is_end() const override { return _iid == _end; }

    Rid rid() const override;

    const Iid &iid() const { return _iid; }
};
