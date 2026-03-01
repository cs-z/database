#pragma once

#include "common.hpp"
#include "page.hpp"

#include <cstdint>
#include <span>

class File
{
public:
    virtual ~File()                                                               = default;
    virtual page::Id getPageCount()                                               = 0;
    virtual void     readPage(page::Id id, std::span<u8, page::SIZE> page)        = 0;
    virtual void     writePage(page::Id id, std::span<const u8, page::SIZE> page) = 0;
    virtual page::Id appendPage()                                                 = 0;
    virtual void     truncate(page::Id newPageCount)                              = 0;
};
