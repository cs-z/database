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

inline bool column_type_is_comparable(ColumnType type)
{
	switch (type) {
		case ColumnType::BOOLEAN: return false;
		case ColumnType::INTEGER: return true;
		case ColumnType::REAL: return true;
		case ColumnType::VARCHAR: return true;
	}
	UNREACHABLE();
}

inline bool column_type_is_arithmetic(ColumnType type)
{
	switch (type) {
		case ColumnType::BOOLEAN: return false;
		case ColumnType::INTEGER: return true;
		case ColumnType::REAL: return true;
		case ColumnType::VARCHAR: return false;
	}
	UNREACHABLE();
}

std::string column_type_to_string(ColumnType type);
void compile_cast(std::optional<ColumnType> from, const std::pair<ColumnType, Text> &to);
std::string column_type_to_catalog_string(ColumnType type);
ColumnType column_type_from_catalog_string(const std::string &name);
