#pragma once

#include "rm/rm_defs.h"

class RmFileHandle;

class RmScan : public RecScan {
  public:
    RmScan(const RmFileHandle *fh);

    void next() override;

    bool is_end() const override { return _rid.page_no == RM_NO_PAGE; }

    Rid rid() const override { return _rid; }

  private:
    const RmFileHandle *_fh;
    Rid _rid;
};
