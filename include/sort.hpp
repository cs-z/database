#pragma once

#include "catalog.hpp"
#include "iter.hpp"
#include "common.hpp"

struct OrderBy
{
	struct Column
	{
		ColumnId column_id;
		bool asc;
	};
	std::vector<Column> columns;
};

struct IterSort : Iter
{
	IterSort(IterPtr parent, OrderBy columns);
	~IterSort() override = default;
	void open() override;
	void close() override;
	std::optional<Value> next() override;

	IterPtr parent;
	const OrderBy columns;

	std::optional<os::TempFile> sorted_file;
	IterPtr sorted_iter;
};
