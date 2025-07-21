#pragma once

#include "common.hpp"
#include "error.hpp"

enum class ColumnType
{
	BOOLEAN,
	INTEGER,
	REAL,
	VARCHAR,
};

inline bool column_type_is_numeric(ColumnType type)
{
	return type == ColumnType::INTEGER || type == ColumnType::REAL;
}

std::string column_type_to_string(ColumnType type);
void column_type_check_cast(std::optional<ColumnType> from, ColumnType to, Text text);
std::string column_type_to_catalog_string(ColumnType type);
ColumnType column_type_from_catalog_string(std::string name);

class Type
{
public:

	inline unsigned int get_column_count() const
	{
		return columns.size();
	}

	inline const auto &get_column(unsigned int id) const
	{
		ASSERT(id < get_column_count());
		return columns[id];
	}

	inline const auto &get_columns() const
	{
		return columns;
	}

	void add_column(std::string name, ColumnType type);
	std::optional<unsigned int> find_column(const std::string &name) const;

private:

	std::vector<std::pair<std::string, ColumnType>> columns;
};
