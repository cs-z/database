#pragma once

#include "common.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace page
{
using Offset = U16;
constexpr Offset kSize{1 << 8}; // TODO

struct IdTag
{
};
using Id = StrongId<IdTag, U32>;

struct EntryIdTag
{
};
using EntryId = StrongId<EntryIdTag, U16>;

template <typename Header = std::monostate, typename EntryInfo = std::monostate> class Slotted
{
public:
    struct Slot
    {
        Offset    offset;
        Offset    size;
        EntryInfo info;
    };

    [[nodiscard]] EntryId GetEntryCount() const
    {
        return entry_count_;
    }

    [[nodiscard]] Header& GetHeader()
    {
        return header_;
    }

    [[nodiscard]] const Header& GetHeader() const
    {
        return header_;
    }

    [[nodiscard]] EntryInfo& GetEntryInfo(EntryId entry_id)
    {
        ASSERT(entry_id < entry_count_);
        return slots_[entry_id.Get()].info;
    }

    [[nodiscard]] const EntryInfo& GetEntryInfo(EntryId entry_id) const
    {
        ASSERT(entry_id < entry_count_);
        return slots_[entry_id.Get()].info;
    }

    [[nodiscard]] const U8* GetEntry(EntryId entry_id) const
    {
        ASSERT(entry_id < entry_count_);
        const Slot& slot = slots_[entry_id.Get()];
        return GetEntry(slot);
    }

    [[nodiscard]] const U8* GetEntry(EntryId entry_id, Offset& size_out) const
    {
        ASSERT(entry_id < entry_count_);
        const Slot& slot = slots_[entry_id.Get()];
        size_out         = slot.size;
        return GetEntry(slot);
    }

    [[nodiscard]] const U8* GetEntry(const Slot& slot) const
    {
        if (slot.offset == 0)
        {
            return nullptr;
        }
        return GetPointer(slot.offset);
    }

    [[nodiscard]] U8* GetEntry(const Slot& slot)
    {
        if (slot.offset == 0)
        {
            return nullptr;
        }
        return GetPointer(slot.offset);
    }

    [[nodiscard]] Slot* Begin()
    {
        return slots_;
    }

    [[nodiscard]] Slot* End()
    {
        return slots_ + entry_count_.Get();
    }

    [[nodiscard]] const Slot* Cbegin() const
    {
        return slots_;
    }

    [[nodiscard]] const Slot* Cend() const
    {
        return slots_ + entry_count_.Get();
    }

    void Init(Header header)
    {
        header_      = std::move(header);
        entry_count_ = EntryId{};
        free_begin_  = offsetof(Slotted, slots_);
        free_end_    = kSize;
    }

    [[nodiscard]] U8* Insert(Offset align, Offset size, EntryInfo info,
                             Offset* free_size_out = nullptr)
    {
        const auto offset = InsertEntry(align, size);
        if (!offset)
        {
            return nullptr;
        }
        ASSERT(*offset % align == 0);

        const EntryId entry_id = entry_count_++;
        slots_[entry_id.Get()] = Slot{*offset, size, std::move(info)};

        if (free_size_out != nullptr)
        {
            *free_size_out = free_end_ - free_begin_;
        }

        return GetPointer(*offset);
    }

    // insert to specific position, shift slots beyond this position
    [[nodiscard]] U8* Insert(Offset align, Offset size, EntryInfo info, EntryId entry_id)
    {
        const auto offset = InsertEntry(align, size);
        if (!offset)
        {
            return nullptr;
        }
        ASSERT(*offset % align == 0);

        std::memmove(slots_ + entry_id.Get() + 1, slots_ + entry_id.Get(),
                     (entry_count_ - entry_id).Get() * sizeof(Slot));
        slots_[entry_id.Get()] = Slot{*offset, size, std::move(info)};
        entry_count_++;

        return GetPointer(*offset);
    }

    void Remove(EntryId entry_id)
    {
        ASSERT(entry_id < entry_count_);
        auto& offset = slots_[entry_id.Get()].offset;
        ASSERT(offset > 0);
        offset = 0;
    }

    void Truncate(EntryId new_entry_count)
    {
        ASSERT(new_entry_count <= entry_count_);
        entry_count_ = new_entry_count;
        free_begin_  = offsetof(Slotted, slots_) + (entry_count_.Get() * sizeof(Slot));
    }

    // shift entries to the end of the page
    void Shift(Offset align)
    {
        std::vector<std::tuple<Offset, Offset, EntryId>> entries;
        for (EntryId entry_id{}; entry_id < entry_count_; entry_id++)
        {
            const Slot& slot = slots_[entry_id.Get()];
            ASSERT(slot.offset % align == 0);
            entries.push_back({slot.offset, slot.size, entry_id});
        }
        std::ranges::sort(entries);

        free_end_ = kSize;
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            auto [offset, size, entry_id] = *it;

            ASSERT(free_end_ > size);
            free_end_ -= size;
            free_end_ = AlignDown(free_end_, align);

            if (free_end_ != offset)
            {
                ASSERT(free_end_ > offset);
                const U8* src = GetPointer(offset);
                U8*       dst = GetPointer(free_end_);
                std::memmove(dst, src, size);
                slots_[entry_id.Get()].offset = free_end_;
            }
        }
    }

private:
    Header header_;

    EntryId entry_count_;
    Offset  free_begin_, free_end_;
    Slot    slots_[kFlexibleArray]; // NOLINT(modernize-avoid-c-arrays)

    [[nodiscard]] U8* GetPointer(Offset offset)
    {
        return reinterpret_cast<U8*>(this) + offset;
    }

    [[nodiscard]] const U8* GetPointer(Offset offset) const
    {
        return reinterpret_cast<const U8*>(this) + offset;
    }

    [[nodiscard]] std::optional<Offset> InsertEntry(Offset align, Offset size)
    {
        ASSERT(align > 0);
        ASSERT(size > 0);

        if (free_end_ < size)
        {
            return std::nullopt;
        }

        const auto free_begin_new = free_begin_ + sizeof(Slot);
        const auto free_end_new   = AlignDown<Offset>(free_end_ - size, align);

        if (free_begin_new > free_end_new)
        {
            return std::nullopt;
        }

        free_begin_ = free_begin_new;
        free_end_   = free_end_new;

        ASSERT(free_end_ % align == 0);
        return free_end_;
    }
};

} // namespace page
