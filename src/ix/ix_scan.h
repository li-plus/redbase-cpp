#pragma once

#include "ix/ix_defs.h"

class IxIndexHandle;

class IxScan : public RecScan {
  public:
    IxScan(const IxIndexHandle *ih, const Iid &lower, const Iid &upper) : _ih(ih), _iid(lower), _end(upper) {}

    void next() override;

    bool is_end() const override { return _iid == _end; }

    Rid rid() const override;

    const Iid &iid() const { return _iid; }

  private:
    const IxIndexHandle *_ih;
    Iid _iid;
    Iid _end;
};
