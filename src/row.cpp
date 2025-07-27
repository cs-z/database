#include "row.hpp"

template <typename T>
constexpr T align_up(T value, T align)
{
	ASSERT(align > 0);
	const T rem = value % align;
	if (rem) {
		value += align - rem;
	}
	return value;
}

namespace row
{
	void Page::init()
	{
		count = 0;
		free_begin = offsetof(Page, slots);
		free_end = PAGE_SIZE.get();
	}

	u8 *Page::insert(Size align, Size size)
	{
		ASSERT(align > 0);
		ASSERT(size > 0);

		ASSERT(size <= get_free_size());

		ASSERT(free_begin % alignof(SlotInfo) == 0);
		ASSERT(free_begin + sizeof(SlotInfo) <= PAGE_SIZE.get());

		SlotInfo &slot = slots[count++];
		free_begin += sizeof(SlotInfo);

		ASSERT(free_end >= size);
		free_end -= size;
		const u32 rem = free_end % align;
		if (rem) {
			ASSERT(free_end >= rem);
			free_end -= rem;
		}
		ASSERT(free_begin <= free_end);

		slot.offset = free_end;
		slot.size = size;

		return reinterpret_cast<u8 *>(this) + free_end;
	}

	bool Page::append(const u8 *row, Size align, Size size)
	{
		ASSERT(row);
		ASSERT(align > 0);
		ASSERT(size > 0);

		u32 free_begin_new = free_begin + sizeof(SlotInfo);
		u32 free_end_new = free_end > size ? free_end - size : 0;
		const u32 rem = free_end_new % align;
		if (rem) {
			ASSERT(free_end_new >= rem);
			free_end_new -= rem;
		}

		if (free_begin_new > free_end_new) {
			return false;
		}

		free_begin = free_begin_new;
		free_end = free_end_new;

		slots[count++] = SlotInfo { free_end, size };

		void * const dest = reinterpret_cast<u8 *>(this) + free_end;
		memcpy(dest, row, size);

		return true;
	}

	u32 calculate_align(const Type &type)
	{
		u32 align = alignof(ColumnPrefix);
		for (ColumnType column_type : type) {
			switch (column_type) {
				case ColumnType::BOOLEAN:
					align = std::max<u32>(align, alignof(ColumnValueBoolean));
					break;
				case ColumnType::INTEGER:
					align = std::max<u32>(align, alignof(ColumnValueInteger));
					break;
				case ColumnType::REAL:
					align = std::max<u32>(align, alignof(ColumnValueReal));
					break;
				case ColumnType::VARCHAR:
					align = std::max<u32>(align, alignof(char));
					break;
			}
		}
		return align;
	}

	Prefix calculate_layout(const Value &value, Size &align_out, Size &size_out)
	{
		align_out = Size(alignof(ColumnPrefix));
		size_out = Size(value.size() * sizeof(ColumnPrefix));
		Prefix prefix;
		for (const ColumnValue &column_value : value) {
			const auto [column_align, column_size] = std::visit(Overload{
				[](const ColumnValueNull &) {
					const u32 column_align = 1;
					const u32 column_size = 0;
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueBoolean &) {
					const u32 column_align = alignof(ColumnValueBoolean);
					const u32 column_size = sizeof(ColumnValueBoolean);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueInteger &) {
					const u32 column_align = alignof(ColumnValueInteger);
					const u32 column_size = sizeof(ColumnValueInteger);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueReal &) {
					const u32 column_align = alignof(ColumnValueReal);
					const u32 column_size = sizeof(ColumnValueReal);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueVarchar &column_value) {
					const u32 column_align = alignof(char);
					const u32 column_size = column_value.size();
					return std::make_pair(column_align, column_size);
				},
			}, column_value);
			align_out = std::max<u32>(align_out, column_align);
			size_out = align_up(size_out, column_align);
			prefix.push_back({ column_size ? size_out : 0, column_size });
			size_out += column_size;
		}
		return prefix;
	}

	template <typename T>
	T *get_column(u8 *row, ColumnId column)
	{
		const u32 offset = reinterpret_cast<ColumnPrefix *>(row)[column.get()].offset;
		return reinterpret_cast<T *>(reinterpret_cast<u8 *>(row) + offset);
	}

	void write(const Prefix &prefix, const Value &value, u8 *row)
	{
		ASSERT(prefix.size() == value.size());
		memcpy(row, prefix.data(), prefix.size() * sizeof(ColumnPrefix));
		for (ColumnId column_id {}; column_id < value.size(); column_id++) {
			std::visit(Overload{
				[&prefix](const ColumnValueNull &) {
				},
				[row, column_id](const ColumnValueBoolean &value) {
					*get_column<ColumnValueBoolean>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueInteger &value) {
					*get_column<ColumnValueInteger>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueReal &value) {
					*get_column<ColumnValueReal>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueVarchar &value) {
					memcpy(get_column<char>(row, column_id), value.data(), value.size());
				},
			}, value[column_id.get()]);
		}
	}

	ColumnPrefix get_prefix(const u8 *row, ColumnId column)
	{
		return reinterpret_cast<const ColumnPrefix *>(row)[column.get()];
	}

	template <typename T>
	const T *get_column(const u8 *row, ColumnPrefix prefix)
	{
		return reinterpret_cast<const T *>(reinterpret_cast<const u8 *>(row) + prefix.offset);
	}

	Value read(const Type &type, const u8 *row)
	{
		Value value;
		for (ColumnId column_id {}; column_id < type.size(); column_id++) {
			const ColumnPrefix prefix = get_prefix(row, column_id);
			if (prefix.offset == 0) {
				value.push_back(ColumnValueNull {});
				continue;
			}
			switch (type[column_id.get()]) {
				case ColumnType::BOOLEAN: {
					value.push_back(*get_column<ColumnValueBoolean>(row, prefix));
					break;
				}
				case ColumnType::INTEGER: {
					value.push_back(*get_column<ColumnValueInteger>(row, prefix));
					break;
				}
				case ColumnType::REAL: {
					value.push_back(*get_column<ColumnValueReal>(row, prefix));
					break;
				}
				case ColumnType::VARCHAR: {
					const char * const begin = get_column<char>(row, prefix);
					value.push_back(ColumnValueVarchar { begin, prefix.size });
					break;
				}
			}
		}
		return value;
	}

	static int compare_strings(const char *string_l, const char *string_r, u32 size_l, u32 size_r)
	{
		const u32 size_min = std::min(size_l, size_r);
		const int result = memcmp(string_l, string_r, size_min);
		if (result) return result;
		if (size_l < size_r) return -1;
		if (size_l > size_r) return +1;
		return 0;
	}

	int compare(const Type &type, ColumnId column, const u8 *row_l, const u8 *row_r)
	{
		// TODO: null values
		const ColumnPrefix prefix_l = get_prefix(row_l, column);
		const ColumnPrefix prefix_r = get_prefix(row_l, column);
		switch (type.at(column.get())) {
			case ColumnType::BOOLEAN: {
				const ColumnValueBoolean column_value_l = *get_column<ColumnValueBoolean>(row_l, prefix_l);
				const ColumnValueBoolean column_value_r = *get_column<ColumnValueBoolean>(row_r, prefix_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::INTEGER: {
				const ColumnValueInteger column_value_l = *get_column<ColumnValueInteger>(row_l, prefix_l);
				const ColumnValueInteger column_value_r = *get_column<ColumnValueInteger>(row_r, prefix_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::REAL: {
				const ColumnValueReal column_value_l = *get_column<ColumnValueReal>(row_l, prefix_l);
				const ColumnValueReal column_value_r = *get_column<ColumnValueReal>(row_r, prefix_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::VARCHAR: {
				const char *column_value_l = get_column<char>(row_l, prefix_l);
				const char *column_value_r = get_column<char>(row_r, prefix_r);
				return compare_strings(column_value_l, column_value_r, prefix_l.size, prefix_r.size);
			}
		}
		UNREACHABLE();
	}
}