#include "value.hpp"

std::optional<ColumnType> column_value_to_type(const ColumnValue &value)
{
	return std::visit(Overload{
		[](const ColumnValueNull &) -> std::optional<ColumnType> {
			return std::nullopt;
		},
		[](const ColumnValueBoolean &) -> std::optional<ColumnType> {
			return ColumnType::BOOLEAN;
		},
		[](const ColumnValueInteger &) -> std::optional<ColumnType> {
			return ColumnType::INTEGER;
		},
		[](const ColumnValueReal &) -> std::optional<ColumnType> {
			return ColumnType::REAL;
		},
		[](const ColumnValueVarchar &) -> std::optional<ColumnType> {
			return ColumnType::VARCHAR;
		},
	}, value);
}

std::string column_value_to_string(const ColumnValue &value, bool quote)
{
	return std::visit(Overload{
		[](const ColumnValueNull &) -> std::string {
			return "NULL";
		},
		[](const ColumnValueBoolean &value) -> std::string {
			switch (value) {
				case Bool::TRUE: return "TRUE";
				case Bool::FALSE: return "FALSE";
				case Bool::UNKNOWN: return "UNKNOWN";
			}
			UNREACHABLE();
		},
		[](const ColumnValueInteger &value) {
			return std::to_string(value);
		},
		[](const ColumnValueReal &value) {
			return std::to_string(value);
		},
		[quote](const ColumnValueVarchar &value) {
			return quote ? ('\'' + value + '\'') : value;
		},
	}, value);
}

ColumnValue column_value_eval_cast(const ColumnValue &value, ColumnType to)
{
	if (to == ColumnType::VARCHAR) {
		return column_value_to_string(value, false);
	}
	return std::visit(Overload{
		[](const ColumnValueNull &) -> ColumnValue {
			UNREACHABLE();
		},
		[to](const ColumnValueBoolean &value) -> ColumnValue {
			switch (to) {
				case ColumnType::INTEGER:
				case ColumnType::REAL:
				case ColumnType::VARCHAR:
					UNREACHABLE();
				case ColumnType::BOOLEAN:
					return value;
			}
			UNREACHABLE();
		},
		[to](const ColumnValueInteger &value) -> ColumnValue {
			switch (to) {
				case ColumnType::BOOLEAN:
				case ColumnType::VARCHAR:
					UNREACHABLE();
				case ColumnType::INTEGER:
					return value;
				case ColumnType::REAL:
					return static_cast<ColumnValueReal>(value);
			}
			UNREACHABLE();
		},
		[to](const ColumnValueReal &value) -> ColumnValue {
			switch (to) {
				case ColumnType::BOOLEAN:
				case ColumnType::VARCHAR:
					UNREACHABLE();
				case ColumnType::INTEGER:
					return static_cast<ColumnValueInteger>(value);
				case ColumnType::REAL:
					return value;
			}
			UNREACHABLE();
		},
		[to](const ColumnValueVarchar &value) -> ColumnValue {
			switch (to) {
				case ColumnType::BOOLEAN:
				case ColumnType::INTEGER:
				case ColumnType::REAL:
					UNREACHABLE();
				case ColumnType::VARCHAR:
					return value;
			}
			UNREACHABLE();
		},
	}, value);
}
