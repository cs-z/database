#pragma once

#include "common.hpp"
#include "page.hpp"
#include "value.hpp"

#include <utility>

static_assert(sizeof(page::Id::Type) + sizeof(page::EntryId::Type) <= sizeof(ColumnValueInteger));

[[nodiscard]] inline ColumnValueInteger PackRowId(page::Id page_id, page::EntryId entry_id)
{
    static constexpr auto kBitOffset = sizeof(page::EntryId::Type) * 8UL;
    return (static_cast<ColumnValueInteger>(page_id.Get()) << kBitOffset) |
           static_cast<ColumnValueInteger>(entry_id.Get());
}

[[nodiscard]] inline std::pair<page::Id, page::EntryId> UnpackRowId(ColumnValueInteger row_id)
{
    ASSERT(row_id >= 0);
    static constexpr auto kBitOffset = sizeof(page::EntryId::Type) * 8UL;
    static constexpr auto kBitMask   = (ColumnValueInteger{1} << kBitOffset) - 1;
    return {static_cast<page::Id>(row_id >> kBitOffset),
            static_cast<page::EntryId>(row_id & kBitMask)};
}
