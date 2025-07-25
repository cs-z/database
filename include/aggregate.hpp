#pragma once

#include "compile.hpp"
#include "iter.hpp"

class Aggregator
{
public:

	void init();
	void feed(const ColumnValue &value);
	ColumnValue get(Function function);

private:

	ColumnValue min;
	ColumnValue max;
	ColumnValue sum;
	ColumnValueInteger count;
};

struct IterAggregate : Iter
{
	IterAggregate(IterPtr parent, Aggregates aggregates);
	~IterAggregate() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	std::optional<std::optional<Value>> feed(const Aggregates::GroupBy &group_by, const std::optional<Value> &value);

	const Aggregates aggregates;

	std::optional<Value> current_key;
	std::vector<Aggregator> aggregators;
	ColumnValueInteger count;
	bool done;

	IterPtr iter;
};
