#pragma once

#include "common.hpp"
#include "page.hpp"

#include <cstdint>
#include <span>

class File
{
public:
    virtual ~File()                                                                = default;
    virtual page::Id GetPageCount()                                                = 0;
    virtual void     ReadPage(page::Id id, std::span<U8, page::kSize> page)        = 0;
    virtual void     WritePage(page::Id id, std::span<const U8, page::kSize> page) = 0;
    virtual page::Id AppendPage()                                                  = 0;
    virtual void     Truncate(page::Id new_page_count)                             = 0;
};
