#pragma once

#include "error.hpp"
#include "type.hpp"

#include <optional>
#include <string>
#include <vector>

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

void Init();

std::string GetFileName(FileId file_id);
FileIds     GetTableFileIds(TableId table_id);

std::pair<TableId, Type>         GetTable(const SourceText& name);
std::pair<TableId, NamedColumns> GetTableNamed(const SourceText& name);

std::optional<std::pair<TableId, Type>>         FindTable(const std::string& name);
std::optional<std::pair<TableId, NamedColumns>> FindTableNamed(const std::string& name);

void CreateTable(std::string name, NamedColumns columns);
void TruncateTable(TableId table_id);
void DropTable(TableId table_id);

[[nodiscard]] Type GetTypeFromNamedColumns(const NamedColumns& named_columns);

// void remove_table(TableId table_id);
} // namespace catalog
