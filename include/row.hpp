#pragma once

#include <algorithm>

#include "common.hpp"
#include "value.hpp"
#include "page.hpp"

namespace row
{
	struct ColumnPrefix
	{
		page::Offset offset;
		page::Offset size;
	};

	using Prefix = std::vector<ColumnPrefix>;

	page::Offset calculate_align(const Type &type);
	Prefix calculate_layout(const Value &value, page::Offset &align_out, page::Offset &size_out);

	void write(const Prefix &prefix, const Value &value, u8 *row);
	Value read(const Type &type, const u8 *row);

	int compare(const Type &type, ColumnId column, const u8 *row_l, const u8 *row_r);
}
