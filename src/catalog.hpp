#pragma once

#include "error.hpp"
#include "type.hpp"

namespace catalog
{
struct FileTag
{
};
using FileId = StrongId<FileTag, unsigned int>;
struct FileIds
{
    FileId fst, dat;
};

struct TableTag
{
};
using TableId = StrongId<TableTag, unsigned int>;

using NamedColumn  = std::pair<std::string, ColumnType>;
using NamedColumns = std::vector<NamedColumn>;

void init();

std::string get_file_name(catalog::FileId file_id);
FileIds     get_table_file_ids(TableId table_id);

std::pair<TableId, Type>         get_table(const SourceText& name);
std::pair<TableId, NamedColumns> get_table_named(const SourceText& name);

std::optional<std::pair<TableId, Type>>         find_table(const std::string& name);
std::optional<std::pair<TableId, NamedColumns>> find_table_named(const std::string& name);

void create_table(std::string name, NamedColumns columns);

// void remove_table(TableId table_id);
}  // namespace catalog
