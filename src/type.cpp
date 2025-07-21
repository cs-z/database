#include "type.hpp"
//#include "parse.hpp" TODO

static const char *tag_to_string(ColumnType::Tag tag)
{
	switch (tag) {
		case ColumnType::NULL_TYPE: return "NULL";
		case ColumnType::BOOLEAN: return "BOOLEAN";
		case ColumnType::INTEGER: return "INTEGER";
		case ColumnType::REAL: return "REAL";
		case ColumnType::VARCHAR: return "VARCHAR";
	}
	UNREACHABLE();
}

std::string ColumnType::to_string() const
{
	std::string string = tag_to_string(tag);
	if (tag == Tag::VARCHAR && size > 0) {
		string += "(" + std::to_string(size) + ")";
	}
	return string;
}

// TODO
//std::string ColumnType::to_catalog_string() const
//{
//	return to_string();
//}

// TODO
//ColumnType ColumnType::from_catalog_string(std::string name)
//{
//	return parse_type(std::move(name));
//}

void ColumnType::check_cast(ColumnType from, ColumnType to, Text text)
{
	switch (from.tag) {
		case ColumnType::NULL_TYPE:
			break;
		case ColumnType::BOOLEAN:
			switch (to.tag) {
				case ColumnType::NULL_TYPE:
				case ColumnType::INTEGER:
				case ColumnType::REAL:
					break;
				case ColumnType::BOOLEAN:
				case ColumnType::VARCHAR:
					return;
			}
			break;
		case ColumnType::INTEGER:
			switch (to.tag) {
				case ColumnType::NULL_TYPE:
				case ColumnType::BOOLEAN:
					break;
				case ColumnType::INTEGER:
				case ColumnType::REAL:
				case ColumnType::VARCHAR:
					return;
			}
			break;
		case ColumnType::REAL:
			switch (to.tag) {
				case ColumnType::NULL_TYPE:
				case ColumnType::BOOLEAN:
					break;
				case ColumnType::INTEGER:
				case ColumnType::REAL:
				case ColumnType::VARCHAR:
					return;
			}
			break;
		case ColumnType::VARCHAR:
			switch (to.tag) {
				case ColumnType::NULL_TYPE:
				case ColumnType::BOOLEAN:
				case ColumnType::INTEGER:
				case ColumnType::REAL:
					break;
				case ColumnType::VARCHAR:
					return;
			}
			break;
	}
	throw ClientError { "invalid cast from " + from.to_string() + " to " + to.to_string(), text };
}

void Type::add_column(std::string name, ColumnType type)
{
	ASSERT(!find_column(name));
	ASSERT(type.tag != ColumnType::NULL_TYPE);
	columns.push_back({ std::move(name), type });
}

std::optional<unsigned int> Type::find_column(const std::string &name) const
{
	for (unsigned int i = 0; i < columns.size(); i++) {
		if (columns[i].first == name) {
			return i;
		}
	}
	return std::nullopt;
}
