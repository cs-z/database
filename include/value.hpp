#pragma once

#include <string_view>

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

inline int compare_strings(std::string_view a, std::string_view b)
{
	const auto size_min = std::min(a.size(), b.size());
	const int result = memcmp(a.data(), b.data(), size_min);
	if (result) return result;
	if (a.size() < b.size()) return -1;
	if (a.size() > b.size()) return +1;
	return 0;
}

using ColumnValue = std::variant<ColumnValueNull, ColumnValueBoolean, ColumnValueInteger, ColumnValueReal, ColumnValueVarchar>;

std::optional<ColumnType> column_value_to_type(const ColumnValue &value);
std::string column_value_to_string(const ColumnValue &value, bool quote);
ColumnValue column_value_eval_cast(const ColumnValue &value, ColumnType to);

using Value = std::vector<ColumnValue>;

void value_print(const Value &value);
std::string value_to_list(const Value &value);
bool value_equal(const Value &a, const Value &b);
