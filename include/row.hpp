#pragma once

#include "common.hpp"
#include "value.hpp"

namespace row
{
	struct ColumnTag {};
	using ColumnId = StrongId<ColumnTag, unsigned int>;

	using Type = std::vector<ColumnType>;

	struct ColumnPrefix
	{
		u32 offset;
		u32 size;
	};

	using Prefix = std::vector<ColumnPrefix>;

	u32 calculate_align(const Type &type);
	Prefix calculate_layout(const Value &value, u32 &align, u32 &size);

	void write(const Prefix &prefix, const Value &value, void *row);
	Value read(const Type &type, const void *row);

	int compare(const Type &type, ColumnId column, const void *row_l, const void *row_r);
}
