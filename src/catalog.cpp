#include "catalog.hpp"
#include "buffer.hpp"
#include "common.hpp"
#include "error.hpp"
#include "execute.hpp"
#include "fst.hpp"
#include "os.hpp"
#include "type.hpp"
#include "value.hpp"

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// TODO: cache catalog data

namespace catalog
{

struct Table
{
    TableId      id;
    std::string  name;
    FileIds      file_ids;
    NamedColumns columns;

    [[nodiscard]] std::string GetDataFileName() const
    {
        return GetDataFileNameFromName(name);
    }
    [[nodiscard]] std::string GetFstFileName() const
    {
        return GetFstFileNameFromName(name);
    }

    [[nodiscard]] static std::string GetDataFileNameFromName(const std::string& name)
    {
        return name + ".DAT";
    }
    [[nodiscard]] static std::string GetFstFileNameFromName(const std::string& name)
    {
        return name + ".FST";
    }
};

// static const Table TABLE_STATS =
// {
// 	.id = TableId { 0 },
// 	.name = "SYS_STATS",
// 	.file_ids = { FileId { 0 }, FileId { 1 } },
// 	.columns = {
// 		{ "FILE_COUNTER", ColumnType::kInteger },
// 		{ "TABLE_COUNTER", ColumnType::kInteger },
// 	},
// };

static const Table kTableFiles = {
    .id       = TableId{1},
    .name     = "SYS_FILES",
    .file_ids = {.fst = FileId{2}, .dat = FileId{3}},
    .columns =
        {
            {"ID", ColumnType::kInteger},
            {"NAME", ColumnType::kVarchar},
        },
};

static const Table kTableTables = {
    .id       = TableId{2},
    .name     = "SYS_TABLES",
    .file_ids = {.fst = FileId{4}, .dat = FileId{5}},
    .columns =
        {
            {"ID", ColumnType::kInteger},
            {"NAME", ColumnType::kVarchar},
            {"FILE_FST_ID", ColumnType::kInteger},
            {"FILE_DAT_ID", ColumnType::kInteger},
        },
};

static const Table kTableColumns = {
    .id       = TableId{3},
    .name     = "SYS_COLUMNS",
    .file_ids = {.fst = FileId{6}, .dat = FileId{7}},
    .columns =
        {
            {"TABLE_ID", ColumnType::kInteger},
            {"ID", ColumnType::kInteger},
            {"NAME", ColumnType::kVarchar},
            {"TYPE", ColumnType::kVarchar},
        },
};

static std::string ReadFile(FileId file_id)
{
    const std::string statement =
        "SELECT NAME FROM " + kTableFiles.name + " WHERE ID = " + file_id.ToString();
    std::vector<Value> values = ExecuteIinternalStatement(statement);
    ASSERT(values.size() == 1);
    auto& name = std::get<ColumnValueVarchar>(values.front().at(0));
    return name;
}

static void WriteFile(FileId file_id, std::string name)
{
    const Value value = {
        ColumnValueInteger{file_id.Get()},
        ColumnValueVarchar{std::move(name)},
    };
    const std::string statement =
        "INSERT INTO " + kTableFiles.name + " VALUES " + ValueToList(value);
    ASSERT(ExecuteIinternalStatement(statement).empty());
}

static std::optional<TableId> ReadTable(const std::string& name)
{
    const std::string statement =
        "SELECT ID FROM " + kTableTables.name + " WHERE NAME = \'" + name + "\'";
    std::vector<Value> values = ExecuteIinternalStatement(statement);
    if (values.empty())
    {
        return std::nullopt;
    }
    ASSERT(values.size() == 1);
    const auto id = std::get<ColumnValueInteger>(values.front().at(0));
    return static_cast<TableId>(id);
}

static FileIds ReadTable(TableId table_id)
{
    const std::string statement = "SELECT FILE_FST_ID, FILE_DAT_ID FROM " + kTableTables.name +
                                  " WHERE ID = " + table_id.ToString();
    std::vector<Value> values = ExecuteIinternalStatement(statement);
    ASSERT(values.size() == 1);
    const auto file_fst = std::get<ColumnValueInteger>(values.front().at(0));
    const auto file_dat = std::get<ColumnValueInteger>(values.front().at(1));
    return {.fst = static_cast<FileId>(file_fst), .dat = static_cast<FileId>(file_dat)};
}

static void WriteTable(TableId table_id, std::string name, FileIds file_ids)
{
    const Value value = {
        ColumnValueInteger{table_id.Get()},
        ColumnValueVarchar{std::move(name)},
        ColumnValueInteger{file_ids.fst.Get()},
        ColumnValueInteger{file_ids.dat.Get()},
    };
    const std::string statement =
        "INSERT INTO " + kTableTables.name + " VALUES " + ValueToList(value);
    ASSERT(ExecuteIinternalStatement(statement).empty());
}

static NamedColumns ReadColumns(TableId table_id)
{
    const std::string statement = "SELECT NAME, TYPE FROM " + kTableColumns.name +
                                  " WHERE TABLE_ID = " + table_id.ToString() + " ORDER BY ID";
    std::vector<Value> values = ExecuteIinternalStatement(statement);
    ASSERT(!values.empty());
    NamedColumns columns;
    for (Value& value : values)
    {
        auto&       column_name = std::get<ColumnValueVarchar>(value.at(0));
        const auto& column_type = std::get<ColumnValueVarchar>(value.at(1));
        columns.emplace_back(std::move(column_name), ColumnTypeFromCatalogString(column_type));
    }
    return columns;
}

static void WriteColumns(TableId table_id, NamedColumns columns)
{
    for (ColumnId column_id{}; column_id < columns.size(); column_id++)
    {
        auto& [column_name, column_type] = columns[column_id.Get()];
        const Value value                = {
            ColumnValueInteger{table_id.Get()},
            ColumnValueInteger{column_id.Get()},
            ColumnValueVarchar{std::move(column_name)},
            ColumnValueVarchar{ColumnTypeToCatalogString(column_type)},
        };
        const std::string statement =
            "INSERT INTO " + kTableColumns.name + " VALUES " + ValueToList(value);
        ASSERT(ExecuteIinternalStatement(statement).empty());
    }
}

std::string GetFileName(FileId file_id)
{
    // if (file_id == TABLE_STATS.file_ids.fst) return TABLE_STATS.GetFstFileName();
    // if (file_id == TABLE_STATS.file_ids.dat) return TABLE_STATS.GetDataFileName();
    if (file_id == kTableFiles.file_ids.fst)
    {
        return kTableFiles.GetFstFileName();
    }
    if (file_id == kTableFiles.file_ids.dat)
    {
        return kTableFiles.GetDataFileName();
    }
    if (file_id == kTableTables.file_ids.fst)
    {
        return kTableTables.GetFstFileName();
    }
    if (file_id == kTableTables.file_ids.dat)
    {
        return kTableTables.GetDataFileName();
    }
    if (file_id == kTableColumns.file_ids.fst)
    {
        return kTableColumns.GetFstFileName();
    }
    if (file_id == kTableColumns.file_ids.dat)
    {
        return kTableColumns.GetDataFileName();
    }
    return ReadFile(file_id);
}

std::optional<std::pair<TableId, Type>> FindTable(const std::string& name)
{
    // TODO: avoid copy
    auto table = FindTableNamed(name);
    if (!table)
    {
        return std::nullopt;
    }
    Type type;
    for (const auto& [column_name, column_type] : table->second)
    {
        type.Push(column_type);
    }
    return std::make_pair(table->first, std::move(type));
}

std::optional<std::pair<TableId, NamedColumns>> FindTableNamed(const std::string& name)
{
    // if (name == TABLE_STATS.name) return std::make_pair(TABLE_STATS.id, TABLE_STATS.columns);
    if (name == kTableFiles.name)
    {
        return std::make_pair(kTableFiles.id, kTableFiles.columns);
    }
    if (name == kTableTables.name)
    {
        return std::make_pair(kTableTables.id, kTableTables.columns);
    }
    if (name == kTableColumns.name)
    {
        return std::make_pair(kTableColumns.id, kTableColumns.columns);
    }
    const std::optional<TableId> table_id = ReadTable(name);
    if (!table_id)
    {
        return std::nullopt;
    }
    return std::make_pair(*table_id, ReadColumns(*table_id));
}

std::pair<TableId, Type> GetTable(const SourceText& name)
{
    auto table = FindTable(name.Get());
    if (!table)
    {
        throw ClientError{"table does not exist", name};
    }
    return std::move(*table);
}

std::pair<TableId, NamedColumns> GetTableNamed(const SourceText& name)
{
    auto table = FindTableNamed(name.Get());
    if (!table)
    {
        throw ClientError{"table does not exist", name};
    }
    return std::move(*table);
}

FileIds GetTableFileIds(TableId table_id)
{
    // if (table_id == TABLE_STATS.id) return TABLE_STATS.file_ids;
    if (table_id == kTableFiles.id)
    {
        return kTableFiles.file_ids;
    }
    if (table_id == kTableTables.id)
    {
        return kTableTables.file_ids;
    }
    if (table_id == kTableColumns.id)
    {
        return kTableColumns.file_ids;
    }
    return ReadTable(table_id);
}

static void CreateTableFiles(const Table& table)
{
    // data file
    os::FileCreate(table.GetDataFileName());

    // fst file
    os::FileCreate(table.GetFstFileName());
    fst::Init(table.file_ids.fst);
}

static void RegisterTable(const Table& table)
{
    // TABLE_FILES
    WriteFile(table.file_ids.fst, table.GetFstFileName());
    WriteFile(table.file_ids.dat, table.GetDataFileName());

    // TABLE_TABLES
    WriteTable(table.id, table.name, table.file_ids);

    // TABLE_COLUMNS
    WriteColumns(table.id, table.columns);
}

void Init()
{
    // TODO: update statement needed
    ASSERT(std::system("rm -rf data") == 0);
    ASSERT(std::system("mkdir -p data") == 0);

    // CreateTableFiles(TABLE_STATS);
    CreateTableFiles(kTableFiles);
    CreateTableFiles(kTableTables);
    CreateTableFiles(kTableColumns);

    // RegisterTable(TABLE_STATS);
    RegisterTable(kTableFiles);
    RegisterTable(kTableTables);
    RegisterTable(kTableColumns);
}

static std::pair<TableId, FileIds> GenerateTableIds()
{
    // TODO: update statement needed
    static auto table_id_todo = TableId{4};
    static auto file_id_todo  = FileId{8};
    return std::make_pair(table_id_todo++, FileIds{.fst = file_id_todo++, .dat = file_id_todo++});
}

void CreateTable(std::string name, NamedColumns columns)
{
    const auto [id, file_ids] = GenerateTableIds();
    const Table table         = {
                .id       = id,
                .name     = std::move(name),
                .file_ids = file_ids,
                .columns  = std::move(columns),
    };
    RegisterTable(table);
    CreateTableFiles(table);
}

void TruncateTable(TableId table_id)
{
    // TODO: clean indexes, metadata, etc
    // Buffer flush? not sure

    // TODO: multiple lookups
    const auto [file_fst, file_dat] = GetTableFileIds(table_id);

    // data file
    os::FileTruncate(GetFileName(file_dat));

    // fst file
    os::FileTruncate(GetFileName(file_fst));
    fst::Init(file_fst);
}

void DropTable(TableId table_id)
{
    ASSERT(table_id != kTableFiles.id);
    ASSERT(table_id != kTableTables.id);
    ASSERT(table_id != kTableColumns.id);

    // TODO: clean indexes, metadata, etc

    const auto [file_fst, file_dat] = GetTableFileIds(table_id);
    const auto file_fst_name        = GetFileName(file_fst);
    const auto file_dat_name        = GetFileName(file_dat);

    std::vector<Value> result;

    const auto statement_columns =
        "DELETE FROM " + kTableColumns.name + " WHERE TABLE_ID = " + table_id.ToString();
    result = ExecuteIinternalStatement(statement_columns);
    ASSERT(result.empty());

    const auto statement_table =
        "DELETE FROM " + kTableTables.name + " WHERE ID = " + table_id.ToString();
    result = ExecuteIinternalStatement(statement_table);
    ASSERT(result.empty());

    const auto statement_files = "DELETE FROM " + kTableFiles.name + " WHERE ID IN (" +
                                 file_fst.ToString() + ", " + file_dat.ToString() + ")";
    result = ExecuteIinternalStatement(statement_files);
    ASSERT(result.empty());

    buffer::Flush(file_fst);
    buffer::Flush(file_dat);

    os::FileRemove(file_fst_name);
    os::FileRemove(file_dat_name);
}

Type GetTypeFromNamedColumns(const NamedColumns& named_columns)
{
    Type type;
    for (const auto& [column_name, column_type] : named_columns)
    {
        type.Push(column_type);
    }
    return type;
}

} // namespace catalog
