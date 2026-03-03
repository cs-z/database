#pragma once

#include "common.hpp"
#include "page.hpp"
#include "value.hpp"

#include <utility>

static_assert(sizeof(page::Id::Type) + sizeof(page::EntryId::Type) <= sizeof(ColumnValueInteger));

inline ColumnValueInteger packRowId(page::Id page_id, page::EntryId entry_id)
{
    static constexpr auto bitOffset = sizeof(page::EntryId::Type) * 8UL;
    return (static_cast<ColumnValueInteger>(page_id.get()) << bitOffset) |
           static_cast<ColumnValueInteger>(entry_id.get());
}

inline std::pair<page::Id, page::EntryId> unpackRowId(ColumnValueInteger rowId)
{
    ASSERT(rowId >= 0);
    static constexpr auto bitOffset = sizeof(page::EntryId::Type) * 8UL;
    static constexpr auto bitMask   = (ColumnValueInteger{1} << bitOffset) - 1;
    return {static_cast<page::Id>(rowId >> bitOffset), static_cast<page::EntryId>(rowId & bitMask)};
}
