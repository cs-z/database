#pragma once

#include "common.hpp"
#include "type.hpp"

enum class Bool
{
	TRUE,
	FALSE,
	UNKNOWN,
};

using ColumnValueNull = std::monostate;
using ColumnValueBoolean = Bool;
using ColumnValueInteger = i64;
using ColumnValueReal = double;
using ColumnValueVarchar = std::string;

using ColumnValue = std::variant<ColumnValueNull, ColumnValueBoolean, ColumnValueInteger, ColumnValueReal, ColumnValueVarchar>;

std::optional<ColumnType> column_value_to_type(const ColumnValue &value);
std::string column_value_to_string(const ColumnValue &value, bool quote);
ColumnValue column_value_eval_cast(const ColumnValue &value, ColumnType to);

using Value = std::vector<ColumnValue>;

void value_print(const Value &value);
bool value_eual(const Value &a, const Value &b);
