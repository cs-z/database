#pragma once

#include "value.hpp"

enum class AggregateFunction
{
	AVG,
	MAX,
	MIN,
	SUM,
	COUNT,
};

inline const char *aggregate_function_cstr(AggregateFunction function)
{
	switch (function) {
		case AggregateFunction::AVG: return "AVG";
		case AggregateFunction::MAX: return "MAX";
		case AggregateFunction::MIN: return "MIN";
		case AggregateFunction::SUM: return "SUM";
		case AggregateFunction::COUNT: return "COUNT";
	}
	UNREACHABLE();
}

class Aggregator
{
public:

	void feed(const ColumnValue *value);
	ColumnValue get(AggregateFunction function);

private:

	ColumnValue min;
	ColumnValue max;
	ColumnValue sum;
	ColumnValueInteger count;

};

//#include "expr.hpp"
//
//struct Aggregate
//{
//	AggregateFunction function;
//	std::unique_ptr<Expr> arg;
//};
//
//using Aggregates = std::vector<Aggregate>;
