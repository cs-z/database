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
			const std::string string = std::to_string(value);
			ASSERT(string.size() > 0);
			std::size_t ptr = string.size() - 1;
			while (ptr > 0 && string[ptr] == '0' && string[ptr - 1] == '0') {
				ptr--;
			}
			if (ptr > 0 && string[ptr] == '0' && string[ptr - 1] != '.') {
				ptr--;
			}
			return string.substr(0, ptr + 1);
		},
		[quote](const ColumnValueVarchar &value) {
			return quote ? ('\'' + value + '\'') : value;
		},
	}, value);
}

ColumnValue column_value_eval_cast(const ColumnValue &value, ColumnType to)
{
	return std::visit(Overload{
		[](const ColumnValueNull &value) -> ColumnValue {
			return value;
		},
		[to](const ColumnValueBoolean &value) -> ColumnValue {
			switch (to) {
				case ColumnType::INTEGER:
				case ColumnType::REAL:
					UNREACHABLE();
				case ColumnType::BOOLEAN:
					return value;
				case ColumnType::VARCHAR:
					return column_value_to_string(value, false);
			}
			UNREACHABLE();
		},
		[to](const ColumnValueInteger &value) -> ColumnValue {
			switch (to) {
				case ColumnType::BOOLEAN:
					UNREACHABLE();
				case ColumnType::INTEGER:
					return value;
				case ColumnType::REAL:
					return static_cast<ColumnValueReal>(value);
				case ColumnType::VARCHAR:
					return column_value_to_string(value, false);
			}
			UNREACHABLE();
		},
		[to](const ColumnValueReal &value) -> ColumnValue {
			switch (to) {
				case ColumnType::BOOLEAN:
					UNREACHABLE();
				case ColumnType::INTEGER:
					return static_cast<ColumnValueInteger>(value);
				case ColumnType::REAL:
					return value;
				case ColumnType::VARCHAR:
					return column_value_to_string(value, false);
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

void value_print(const Value &value)
{
	printf("(");
	for (std::size_t i = 0; i < value.size(); i++) {
		printf("%s", column_value_to_string(value[i], true).c_str());
		if (i + 1 < value.size()) {
			printf(", ");
		}
	}
	printf(")");
}

std::string value_to_list(const Value &value)
{
	std::string list = "(";
	for (std::size_t i = 0; i < value.size(); i++) {
		list += column_value_to_string(value[i], true);
		if (i + 1< value.size()) {
			list += ", ";
		}
	}
	return list + ")";
}

bool value_equal(const Value &a, const Value &b)
{
	ASSERT(a.size() == b.size());
	for (std::size_t i = 0; i < a.size(); i++) {
		if (a[i] != b[i]) {
			return false;
		}
	}
	return true;
}
