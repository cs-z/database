#include "aggregate.hpp"
#include "sort.hpp"

void Aggregator::init()
{
	min = {};
	max = {};
	sum = {};
	count = {};
}

void Aggregator::feed(const ColumnValue &value)
{
	std::visit(Overload{
		[](const ColumnValueNull &) {
			// ignore null values
		},
		[this](const ColumnValueBoolean &) {
			UNREACHABLE();
		},
		[this](const ColumnValueInteger &value) {
			if (this->count == 0) {
				this->min = value;
				this->max = value;
				this->sum = value;
			}
			else {
				this->min = std::min(std::get<ColumnValueInteger>(this->min), value);
				this->max = std::max(std::get<ColumnValueInteger>(this->max), value);
				this->sum = std::get<ColumnValueInteger>(this->sum) + value;
			}
			this->count++;
		},
		[this](const ColumnValueReal &value) {
			if (this->count == 0) {
				this->min = value;
				this->max = value;
				this->sum = value;
			}
			else {
				this->min = std::min(std::get<ColumnValueReal>(this->min), value);
				this->max = std::max(std::get<ColumnValueReal>(this->max), value);
				sum = std::get<ColumnValueReal>(this->sum) + value;
			}
			this->count++;
		},
		[this](const ColumnValueVarchar &value) {
			if (this->count == 0) {
				this->min = value;
				this->max = value;
			}
			else {
				if (compare_strings(value, std::get<ColumnValueVarchar>(this->min)) < 0) {
					this->min = value;
				}
				if (compare_strings(value, std::get<ColumnValueVarchar>(this->max)) > 0) {
					this->max = value;
				}
			}
			this->count++;
		},
	}, value);
}

ColumnValue Aggregator::get(Function function)
{
	switch (function) {
		case Function::AVG:
			return std::visit(Overload{
				[](const ColumnValueNull &value) -> ColumnValue {
					return value;
				},
				[](const ColumnValueBoolean &) -> ColumnValue {
					UNREACHABLE();
				},
				[this](const ColumnValueInteger &value) -> ColumnValue {
					ASSERT(this->count > 0);
					return value / this->count;
				},
				[this](const ColumnValueReal &value) -> ColumnValue {
					ASSERT(this->count > 0);
					return value / this->count;
				},
				[](const ColumnValueVarchar &) -> ColumnValue {
					UNREACHABLE();
				},
			}, sum);
		case Function::MAX:
			return max;
		case Function::MIN:
			return min;
		case Function::SUM:
			return sum;
		case Function::COUNT:
			return count;
	}
	UNREACHABLE();
}

IterAggregate::IterAggregate(IterPtr parent, Aggregates aggregates)
	: Iter { parent->type }
	, aggregates { std::move(aggregates) }
	, aggregators { this->aggregates.exprs.size() }
{
	if (this->aggregates.group_by.size() > 0) {
		OrderBy order_by;
		for (ColumnId column_id : this->aggregates.group_by) {
			order_by.columns.push_back({ column_id, true });
		}
		iter = std::make_unique<IterSort>(std::move(parent), std::move(order_by));
	}
	else {
		iter = std::move(parent);
	}
}

void IterAggregate::open()
{
	done = false;
	iter->open();
}

void IterAggregate::close()
{
	iter->close();
}

std::optional<std::optional<Value>> IterAggregate::feed(const Aggregates::GroupBy &group_by, const std::optional<Value> &value)
{
	// TODO

	// vars: old?, new?
	// return null: (!old && !new)
	// return value: (old && !new) || (old && new && old != new)
	// init if: !old || (old && new && old != new)
	// feed if: new

	std::optional<Value> value_key;
	if (value) {
		value_key = Value {};
		for (ColumnId key_id : group_by) {
			value_key->push_back(value->at(key_id.get()));
		}
	}

	const bool should_return_null = !current_key && !value_key;
	const bool key_changed = current_key && value_key && !value_equal(*current_key, *value_key);
	const bool should_return_value = (current_key && !value_key) || key_changed;
	const bool should_init = !current_key || key_changed;
	const bool should_feed = value_key.has_value();

	std::optional<std::optional<Value>> result;

	if (should_return_null) {
		result = std::optional<Value> { std::nullopt };
	}

	if (should_return_value) {
		ASSERT(current_key);
		Value result_value = std::move(*current_key);
		for (std::size_t i = 0; i < aggregates.exprs.size(); i++) {
			if (aggregates.exprs[i].arg) {
				result_value.push_back(aggregators[i].get(aggregates.exprs[i].function));
			}
			else {
				result_value.push_back(count);
			}
		}
		result = { result_value };
	}

	if (should_init) {
		if (value_key) {
			current_key = std::move(value_key);
		}
		for (Aggregator &aggregator : aggregators) {
			aggregator.init();
		}
		count = 0;
	}

	if (should_feed) {
		ASSERT(value);
		for (std::size_t i = 0; i < aggregates.exprs.size(); i++) {
			if (aggregates.exprs[i].arg) {
				const ColumnValue column_value = aggregates.exprs[i].arg->eval(&*value);
				aggregators[i].feed(column_value);
			}
		}
		count++;
	}

	return result;
}

std::optional<Value> IterAggregate::next()
{
	if (done) {
		return std::nullopt;
	}
	if (aggregates.group_by.size() > 0) {
		for (;;) {
			const std::optional<Value> value = iter->next();
			if (!value) {
				done = true;
			}
			const std::optional<std::optional<Value>> result = feed(aggregates.group_by, value);
			if (result) {
				return *result;
			}
		}
	}
	else {
		for (Aggregator &aggregator : aggregators) {
			aggregator.init();
		}
		count = 0;
		for (;;) {
			const std::optional<Value> value = iter->next();
			if (!value) {
				break;
			}
			for (std::size_t i = 0; i < aggregates.exprs.size(); i++) {
				if (aggregates.exprs[i].arg) {
					const ColumnValue column_value = aggregates.exprs[i].arg->eval(&*value);
					aggregators[i].feed(column_value);
				}
			}
			count++;
		}
		Value result;
		for (std::size_t i = 0; i < aggregates.exprs.size(); i++) {
			if (aggregates.exprs[i].arg) {
				result.push_back(aggregators[i].get(aggregates.exprs[i].function));
			}
			else {
				result.push_back(count);
			}
		}
		done = true;
		return result;
	}
}
