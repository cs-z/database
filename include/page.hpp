#pragma once

#include <algorithm>

#include "common.hpp"

namespace page
{
	struct IdTag {};
	using Id = StrongId<IdTag, u32>;

	using Offset = u16;

	struct SlotIdTag {};
	using SlotId = StrongId<SlotIdTag, u32>;

	constexpr Offset SIZE { 1 << 7 }; // TODO

	class PageSlotted
	{
	public:

		struct Slot
		{
			Offset offset;
			Offset size;
		};

		inline Offset get_free_size() const
		{
			const Offset size = free_end - free_begin;
			return size > sizeof(Slot) ? size - sizeof(Slot) : Offset { 0 };
		}

		inline unsigned int get_row_count() const { return slots_count; }

		inline u8 *get_row(SlotId slot_id)
		{
			auto [offset, size] = slots[slot_id.get()];
			if (!offset) return nullptr;
			return reinterpret_cast<u8 *>(this) + offset;
		}

		inline const u8 *get_row(SlotId slot_id) const
		{
			auto [offset, size] = slots[slot_id.get()];
			if (!offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + offset;
		}

		inline const u8 *get_row(SlotId slot_id, Offset &size_out) const
		{
			auto [offset, size] = slots[slot_id.get()];
			size_out = size;
			if (!offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + offset;
		}

		const u8 *get_row(const Slot &slot) const
		{
			if (!slot.offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + slot.offset;
		}

		void init();

		u8 *insert(Offset align, Offset size);

		bool append(const u8 *row, Offset align, Offset size);

		template <typename Compare>
		void sort(Compare compare)
		{
			std::sort(slots, slots + slots_count, compare);
		}

	private:

		unsigned int slots_count;
		Offset free_begin, free_end;

		Slot slots[FLEXIBLE_ARRAY];
	};

}
