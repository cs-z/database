#pragma once

#include "type.hpp"
#include "error.hpp"

namespace catalog
{
	struct FileTag {};
	using FileId = StrongId<FileTag, unsigned int>;

	struct TableTag {};
	using TableId = StrongId<TableTag, unsigned int>;

	void init();

	struct ColumnDef
	{
		std::string name;
		ColumnType type;
	};

	using TableDef = std::vector<ColumnDef>;

	std::string get_file_name(catalog::FileId file);
	std::pair<FileId, FileId> get_table_files(TableId table_id);
	std::pair<TableId, TableDef> get_table(const SourceText &table_name);
	std::optional<std::pair<TableId, TableDef>> find_table(const std::string &table_name);

	void create_table(std::string table_name, TableDef table_def);
	//void remove_table(TableId table_id);
}
