#pragma once

#include <algorithm>
#include <array>

#include "common.hpp"

namespace page
{
using Offset = u16;
constexpr Offset SIZE{1 << 8};  // TODO

struct IdTag
{
};
using Id = StrongId<IdTag, u32>;

struct EntryIdTag
{
};
using EntryId = StrongId<EntryIdTag, u16>;

template <typename Header = std::monostate, typename EntryInfo = std::monostate> class Slotted
{
  public:
    struct Slot
    {
        Offset    offset;
        Offset    size;
        EntryInfo info;
    };

    [[nodiscard]] EntryId get_entry_count() const { return entry_count; }

    [[nodiscard]] Header& get_header() { return header; }

    [[nodiscard]] const Header& get_header() const { return header; }

    [[nodiscard]] EntryInfo& get_entry_info(EntryId entry_id)
    {
        ASSERT(entry_id < entry_count);
        return slots[entry_id.get()].info;
    }

    [[nodiscard]] const EntryInfo& get_entry_info(EntryId entry_id) const
    {
        ASSERT(entry_id < entry_count);
        return slots[entry_id.get()].info;
    }

    [[nodiscard]] const u8* get_entry(EntryId entry_id) const
    {
        ASSERT(entry_id < entry_count);
        const Slot& slot = slots[entry_id.get()];
        return get_entry(slot);
    }

    [[nodiscard]] const u8* get_entry(EntryId entry_id, Offset& size_out) const
    {
        ASSERT(entry_id < entry_count);
        const Slot& slot = slots[entry_id.get()];
        size_out         = slot.size;
        return get_entry(slot);
    }

    [[nodiscard]] const u8* get_entry(const Slot& slot) const { return get_pointer(slot.offset); }

    [[nodiscard]] u8* get_entry(const Slot& slot) { return get_pointer(slot.offset); }

    [[nodiscard]] Slot* begin() { return slots; }

    [[nodiscard]] Slot* end() { return slots + entry_count.get(); }

    [[nodiscard]] const Slot* cbegin() const { return slots; }

    [[nodiscard]] const Slot* cend() const { return slots + entry_count.get(); }

    void init(Header header)
    {
        this->header      = std::move(header);
        this->entry_count = EntryId{};
        this->free_begin  = offsetof(Slotted, slots);
        this->free_end    = SIZE;
    }

    [[nodiscard]] u8* insert(Offset align, Offset size, EntryInfo info,
                             Offset* free_size_out = nullptr)
    {
        const auto offset = insert_entry(align, size);
        if (!offset)
        {
            return nullptr;
        }
        ASSERT(*offset % align == 0);

        const EntryId entry_id = entry_count++;
        slots[entry_id.get()]  = Slot{*offset, size, std::move(info)};

        if (free_size_out != nullptr)
        {
            *free_size_out = free_end - free_begin;
        }

        return get_pointer(*offset);
    }

    // insert to specific position, shift slots beyond this position
    [[nodiscard]] u8* insert(Offset align, Offset size, EntryInfo info, EntryId entry_id)
    {
        const auto offset = insert_entry(align, size);
        if (!offset)
        {
            return nullptr;
        }
        ASSERT(*offset % align == 0);

        memmove(slots + entry_id.get() + 1, slots + entry_id.get(),
                (entry_count - entry_id).get() * sizeof(Slot));
        slots[entry_id.get()] = Slot{*offset, size, std::move(info)};
        entry_count++;

        return get_pointer(*offset);
    }

    void remove_beyond(EntryId new_entry_count)
    {
        ASSERT(new_entry_count <= entry_count);
        entry_count = new_entry_count;
        free_begin  = offsetof(Slotted, slots) + entry_count.get() * sizeof(Slot);
    }

    // shift entries to the end of the page
    void shift(Offset align)
    {
        std::vector<std::tuple<Offset, Offset, EntryId>> entries;
        for (EntryId entry_id{}; entry_id < entry_count; entry_id++)
        {
            const Slot& slot = slots[entry_id.get()];
            ASSERT(slot.offset % align == 0);
            entries.push_back({slot.offset, slot.size, entry_id});
        }
        std::ranges::sort(entries);

        free_end = SIZE;
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            auto [offset, size, entry_id] = *it;

            ASSERT(free_end > size);
            free_end -= size;
            free_end = align_down(free_end, align);

            if (free_end != offset)
            {
                ASSERT(free_end > offset);
                const u8* src = get_pointer(offset);
                u8*       dst = get_pointer(free_end);
                memmove(dst, src, size);
                slots[entry_id.get()].offset = free_end;
            }
        }
    }

  private:
    Header header;

    EntryId entry_count;
    Offset  free_begin, free_end;
    Slot    slots[FLEXIBLE_ARRAY];  // NOLINT(modernize-avoid-c-arrays)

    [[nodiscard]] u8* get_pointer(Offset offset) { return reinterpret_cast<u8*>(this) + offset; }

    [[nodiscard]] const u8* get_pointer(Offset offset) const
    {
        return reinterpret_cast<const u8*>(this) + offset;
    }

    [[nodiscard]] std::optional<Offset> insert_entry(Offset align, Offset size)
    {
        ASSERT(align > 0);
        ASSERT(size > 0);

        if (free_end < size)
        {
            return std::nullopt;
        }

        const auto free_begin_new = free_begin + sizeof(Slot);
        const auto free_end_new   = align_down<Offset>(free_end - size, align);

        if (free_begin_new > free_end_new)
        {
            return std::nullopt;
        }

        free_begin = free_begin_new;
        free_end   = free_end_new;

        ASSERT(free_end % align == 0);
        return free_end;
    }
};

}  // namespace page
