#include "aggregate.hpp"

void Aggregator::feed(const ColumnValue *value)
{
	if (value) {
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
				}
				else {
					this->min = std::min(std::get<ColumnValueInteger>(this->min), value);
					this->max = std::max(std::get<ColumnValueInteger>(this->max), value);
				}
				sum = std::get<ColumnValueInteger>(this->sum) + value;
				this->count++;
			},
			[this](const ColumnValueReal &value) {
				if (this->count == 0) {
					this->min = value;
					this->max = value;
				}
				else {
					this->min = std::min(std::get<ColumnValueReal>(this->min), value);
					this->max = std::max(std::get<ColumnValueReal>(this->max), value);
				}
				sum = std::get<ColumnValueReal>(this->sum) + value;
				this->count++;
			},
			[this](const ColumnValueVarchar &value) {
				if (this->count == 0) {
					this->min = value;
					this->max = value;
				}
				else {
					this->min = std::min(std::get<ColumnValueVarchar>(this->min), value);
					this->max = std::max(std::get<ColumnValueVarchar>(this->max), value);
				}
				this->count++;
			},
		}, *value);
	}
	else {
		count++;
	}
}

ColumnValue Aggregator::get(AggregateFunction function)
{
	switch (function) {
		case AggregateFunction::AVG:
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
		case AggregateFunction::MAX:
			return max;
		case AggregateFunction::MIN:
			return min;
		case AggregateFunction::SUM:
			return sum;
		case AggregateFunction::COUNT:
			return count;
	}
	UNREACHABLE();
}
