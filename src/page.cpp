#include "page.hpp"

namespace page
{

	void PageSlotted::init()
	{
		slots_count = 0;
		free_begin = offsetof(PageSlotted, slots);
		free_end = SIZE;
	}

	u8 *PageSlotted::insert(Offset align, Offset size)
	{
		ASSERT(align > 0);
		ASSERT(size > 0);

		ASSERT(size <= get_free_size());

		ASSERT(free_begin % alignof(Slot) == 0);
		ASSERT(free_begin + sizeof(Slot) <= SIZE);

		Slot &slot = slots[slots_count++];
		free_begin += sizeof(Slot);

		ASSERT(free_end >= size);
		free_end -= size;
		const Offset rem = free_end % align;
		if (rem) {
			ASSERT(free_end >= rem);
			free_end -= rem;
		}
		ASSERT(free_begin <= free_end);

		slot.offset = free_end;
		slot.size = size;

		return reinterpret_cast<u8 *>(this) + free_end;
	}

	bool PageSlotted::append(const u8 *row, Offset align, Offset size)
	{
		ASSERT(row);
		ASSERT(align > 0);
		ASSERT(size > 0);

		Offset free_begin_new = free_begin + sizeof(Slot);
		Offset free_end_new = free_end > size ? free_end - size : 0;
		const Offset rem = free_end_new % align;
		if (rem) {
			ASSERT(free_end_new >= rem);
			free_end_new -= rem;
		}

		if (free_begin_new > free_end_new) {
			return false;
		}

		free_begin = free_begin_new;
		free_end = free_end_new;

		slots[slots_count++] = Slot { free_end, size };

		void * const dest = reinterpret_cast<u8 *>(this) + free_end;
		memcpy(dest, row, size);

		return true;
	}
}
