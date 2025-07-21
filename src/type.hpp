#pragma once

#include "common.hpp"
#include "error.hpp"

struct ColumnType
{
	enum Tag
	{
		NULL_TYPE,
		BOOLEAN,
		INTEGER,
		REAL,
		VARCHAR,
	};

	Tag tag;
	unsigned int size;

	inline bool operator==(const ColumnType &other) const { return tag == other.tag; }
	inline bool is_numeric() const { return tag == INTEGER || tag == REAL; }

	std::string to_string() const;
	// TODO
	//std::string to_catalog_string() const;

	// TODO
	//static ColumnType from_catalog_string(std::string name);
	static void check_cast(ColumnType from, ColumnType to, Text text);
};

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
