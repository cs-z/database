#pragma once

#include "common.hpp"
#include "value.hpp"

namespace row
{
	using Size = u32;

	class Page
	{
	public:

		struct SlotTag {};
		using Slot = StrongId<SlotTag, u32>;

		struct SlotInfo
		{
			u32 offset;
			u32 size;
		};

		inline Size get_free_size() const
		{
			const Size size = free_end - free_begin;
			return size > sizeof(SlotInfo) ? size - sizeof(SlotInfo) : 0;
		}

		inline unsigned int get_row_count() const { return count; }

		inline u8 *get_row(Slot slot)
		{
			auto [offset, size] = slots[slot.get()];
			if (!offset) return nullptr;
			return reinterpret_cast<u8 *>(this) + offset;
		}

		inline const u8 *get_row(Slot slot) const
		{
			auto [offset, size] = slots[slot.get()];
			if (!offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + offset;
		}

		inline const u8 *get_row(Slot slot, Size &size_out) const
		{
			auto [offset, size] = slots[slot.get()];
			size_out = size;
			if (!offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + offset;
		}

		const u8 *get_row(SlotInfo slot) const
		{
			if (!slot.offset) return nullptr;
			return reinterpret_cast<const u8 *>(this) + slot.offset;
		}

		void init();
		u8 *insert(Size align, Size size);
		bool append(const u8 *row, Size align, Size size);

		template <typename Compare>
		void sort(Compare compare)
		{
			std::sort(slots, slots + count, compare);
		}

	private:

		unsigned int count;
		Size free_begin;
		Size free_end;

		SlotInfo slots[1];
	};

	struct ColumnPrefix
	{
		u32 offset;
		u32 size;
	};

	using Prefix = std::vector<ColumnPrefix>;

	Size calculate_align(const Type &type);
	Prefix calculate_layout(const Value &value, Size &align_out, Size &size_out);

	void write(const Prefix &prefix, const Value &value, u8 *row);
	Value read(const Type &type, const u8 *row);

	int compare(const Type &type, ColumnId column, const u8 *row_l, const u8 *row_r);
}
