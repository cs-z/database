#pragma once

#include "type.hpp"
#include "error.hpp"

namespace catalog
{
	struct FileTag {};
	using FileId = StrongId<FileTag, unsigned int>;

	struct TableTag {};
	using TableId = StrongId<TableTag, unsigned int>;

	using NamedColumn = std::pair<std::string, ColumnType>;
	using NamedColumns = std::vector<NamedColumn>;

	void init();

	std::string get_file_name(catalog::FileId file);
	std::pair<FileId, FileId> get_table_files(TableId table_id);

	std::pair<TableId, Type> get_table(const SourceText &table_name);
	std::pair<TableId, NamedColumns> get_table_named(const SourceText &table_name);

	std::optional<std::pair<TableId, Type>> find_table(const std::string &table_name);
	std::optional<std::pair<TableId, NamedColumns>> find_table_named(const std::string &table_name);

	void create_table(std::string table_name, NamedColumns columns);
	//void remove_table(TableId table_id);
}
