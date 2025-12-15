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

class IterSort : public IterBase
{
public:

	IterSort(Iter parent, OrderBy columns)
		: IterBase { parent->type }
		, parent { std::move(parent) }
		, columns { std::move(columns) }
	{
	}
	~IterSort() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	Iter parent;
	const OrderBy columns;

	Iter sorted_iter;
};
