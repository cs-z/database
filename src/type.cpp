#include "type.hpp"
#include "parse.hpp"

std::string column_type_to_string(ColumnType type)
{
	switch (type) {
		case ColumnType::BOOLEAN: return "BOOLEAN";
		case ColumnType::INTEGER: return "INTEGER";
		case ColumnType::REAL: return "REAL";
		case ColumnType::VARCHAR: return "VARCHAR";
	}
	UNREACHABLE();
}

std::string column_type_to_catalog_string(ColumnType type)
{
	return column_type_to_string(type);
}

ColumnType column_type_from_catalog_string(const std::string &name)
{
	return parse_type(name);
}

void compile_cast(std::optional<ColumnType> from, const std::pair<ColumnType, SourceText> &to)
{
	if (from) {
		switch (*from) {
			case ColumnType::BOOLEAN:
				switch (to.first) {
					case ColumnType::INTEGER:
					case ColumnType::REAL:
						break;
					case ColumnType::BOOLEAN:
					case ColumnType::VARCHAR:
						return;
				}
				break;
			case ColumnType::INTEGER:
				switch (to.first) {
					case ColumnType::BOOLEAN:
						break;
					case ColumnType::INTEGER:
					case ColumnType::REAL:
					case ColumnType::VARCHAR:
						return;
				}
				break;
			case ColumnType::REAL:
				switch (to.first) {
					case ColumnType::BOOLEAN:
						break;
					case ColumnType::INTEGER:
					case ColumnType::REAL:
					case ColumnType::VARCHAR:
						return;
				}
				break;
			case ColumnType::VARCHAR:
				switch (to.first) {
					case ColumnType::BOOLEAN:
					case ColumnType::INTEGER:
					case ColumnType::REAL:
						break;
					case ColumnType::VARCHAR:
						return;
				}
				break;
		}
	}
	throw ClientError { "invalid cast from " + (from ? column_type_to_string(*from) : "NULL") + " to " + column_type_to_string(to.first), to.second };
}
