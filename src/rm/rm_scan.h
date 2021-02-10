#pragma once

#include "rm_defs.h"
#include <memory>

class RmFileHandle;

class RmScan : public RecScan {
    std::shared_ptr<RmFileHandle> _fh;
    Rid _rid;
public:
    RmScan(std::shared_ptr<RmFileHandle> fh);

    void next() override;

    bool is_end() const override { return _rid.page_no == RM_NO_PAGE; }

    Rid rid() const override { return _rid; }
};
