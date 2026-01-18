#include "catalog.hpp"
#include "execute.hpp"
#include "fst.hpp"
#include "os.hpp"
#include "value.hpp"

// TODO: cache catalog data

namespace catalog
{

struct Table
{
    TableId      id;
    std::string  name;
    FileIds      file_ids;
    NamedColumns columns;

    [[nodiscard]] std::string getDataFileName() const { return name + ".DAT"; }

    [[nodiscard]] std::string getFstFileName() const { return name + ".FST"; }
};

// static const Table TABLE_STATS =
// {
// 	.id = TableId { 0 },
// 	.name = "SYS_STATS",
// 	.file_ids = { FileId { 0 }, FileId { 1 } },
// 	.columns = {
// 		{ "FILE_COUNTER", ColumnType::INTEGER },
// 		{ "TABLE_COUNTER", ColumnType::INTEGER },
// 	},
// };

static const Table TABLE_FILES = {
    .id       = TableId{1},
    .name     = "SYS_FILES",
    .file_ids = {FileId{2}, FileId{3}},
    .columns =
        {
            {"ID", ColumnType::INTEGER},
            {"NAME", ColumnType::VARCHAR},
        },
};

static const Table TABLE_TABLES = {
    .id       = TableId{2},
    .name     = "SYS_TABLES",
    .file_ids = {FileId{4}, FileId{5}},
    .columns =
        {
            {"ID", ColumnType::INTEGER},
            {"NAME", ColumnType::VARCHAR},
            {"FILE_FST_ID", ColumnType::INTEGER},
            {"FILE_DAT_ID", ColumnType::INTEGER},
        },
};

static const Table TABLE_COLUMNS = {
    .id       = TableId{3},
    .name     = "SYS_COLUMNS",
    .file_ids = {FileId{6}, FileId{7}},
    .columns =
        {
            {"TABLE_ID", ColumnType::INTEGER},
            {"ID", ColumnType::INTEGER},
            {"NAME", ColumnType::VARCHAR},
            {"TYPE", ColumnType::VARCHAR},
        },
};

static std::string read_file(FileId file_id)
{
    const std::string statement =
        "SELECT NAME FROM " + TABLE_FILES.name + " WHERE ID = " + file_id.to_string();
    puts(statement.c_str());
    std::vector<Value> values = execute_internal_statement(statement);
    ASSERT(values.size() == 1);
    auto& name = std::get<ColumnValueVarchar>(values.front().at(0));
    return name;
}

static void write_file(FileId file_id, std::string name)
{
    const Value value = {
        ColumnValueInteger{file_id.get()},
        ColumnValueVarchar{std::move(name)},
    };
    const std::string statement =
        "INSERT INTO " + TABLE_FILES.name + " VALUES " + value_to_list(value);
    ASSERT(execute_internal_statement(statement).empty());
}

static std::optional<TableId> read_table(const std::string& name)
{
    const std::string statement =
        "SELECT ID FROM " + TABLE_TABLES.name + " WHERE NAME = \'" + name + "\'";
    std::vector<Value> values = execute_internal_statement(statement);
    if (values.empty())
    {
        return std::nullopt;
    }
    ASSERT(values.size() == 1);
    const auto id = std::get<ColumnValueInteger>(values.front().at(0));
    return static_cast<TableId>(id);
}

static FileIds read_table(TableId table_id)
{
    const std::string statement = "SELECT FILE_FST_ID, FILE_DAT_ID FROM " + TABLE_TABLES.name +
                                  " WHERE ID = " + table_id.to_string();
    std::vector<Value> values = execute_internal_statement(statement);
    ASSERT(values.size() == 1);
    const auto file_fst = std::get<ColumnValueInteger>(values.front().at(0));
    const auto file_dat = std::get<ColumnValueInteger>(values.front().at(1));
    return {static_cast<FileId>(file_fst), static_cast<FileId>(file_dat)};
}

static void write_table(TableId table_id, std::string name, FileIds file_ids)
{
    const Value value = {
        ColumnValueInteger{table_id.get()},
        ColumnValueVarchar{std::move(name)},
        ColumnValueInteger{file_ids.fst.get()},
        ColumnValueInteger{file_ids.dat.get()},
    };
    const std::string statement =
        "INSERT INTO " + TABLE_TABLES.name + " VALUES " + value_to_list(value);
    ASSERT(execute_internal_statement(statement).empty());
}

static NamedColumns read_columns(TableId table_id)
{
    const std::string statement = "SELECT NAME, TYPE FROM " + TABLE_COLUMNS.name +
                                  " WHERE TABLE_ID = " + table_id.to_string() + " ORDER BY ID";
    std::vector<Value> values = execute_internal_statement(statement);
    ASSERT(!values.empty());
    NamedColumns columns;
    for (Value& value : values)
    {
        auto&       column_name = std::get<ColumnValueVarchar>(value.at(0));
        const auto& column_type = std::get<ColumnValueVarchar>(value.at(1));
        columns.emplace_back(std::move(column_name), column_type_from_catalog_string(column_type));
    }
    return columns;
}

static void write_columns(TableId table_id, NamedColumns columns)
{
    for (ColumnId column_id{}; column_id < columns.size(); column_id++)
    {
        auto& [column_name, column_type] = columns[column_id.get()];
        const Value value                = {
            ColumnValueInteger{table_id.get()},
            ColumnValueInteger{column_id.get()},
            ColumnValueVarchar{std::move(column_name)},
            ColumnValueVarchar{column_type_to_catalog_string(column_type)},
        };
        const std::string statement =
            "INSERT INTO " + TABLE_COLUMNS.name + " VALUES " + value_to_list(value);
        ASSERT(execute_internal_statement(statement).empty());
    }
}

std::string get_file_name(catalog::FileId file_id)
{
    // if (file_id == TABLE_STATS.file_ids.fst) return TABLE_STATS.getFstFileName();
    // if (file_id == TABLE_STATS.file_ids.dat) return TABLE_STATS.getDataFileName();
    if (file_id == TABLE_FILES.file_ids.fst)
    {
        return TABLE_FILES.getFstFileName();
    }
    if (file_id == TABLE_FILES.file_ids.dat)
    {
        return TABLE_FILES.getDataFileName();
    }
    if (file_id == TABLE_TABLES.file_ids.fst)
    {
        return TABLE_TABLES.getFstFileName();
    }
    if (file_id == TABLE_TABLES.file_ids.dat)
    {
        return TABLE_TABLES.getDataFileName();
    }
    if (file_id == TABLE_COLUMNS.file_ids.fst)
    {
        return TABLE_COLUMNS.getFstFileName();
    }
    if (file_id == TABLE_COLUMNS.file_ids.dat)
    {
        return TABLE_COLUMNS.getDataFileName();
    }
    return read_file(file_id);
}

std::optional<std::pair<TableId, Type>> find_table(const std::string& name)
{
    // TODO: avoid copy
    auto table = find_table_named(name);
    if (!table)
    {
        return std::nullopt;
    }
    Type type;
    for (const auto& [column_name, column_type] : table->second)
    {
        type.push(column_type);
    }
    return std::make_pair(table->first, std::move(type));
}

std::optional<std::pair<TableId, NamedColumns>> find_table_named(const std::string& name)
{
    // if (name == TABLE_STATS.name) return std::make_pair(TABLE_STATS.id, TABLE_STATS.columns);
    if (name == TABLE_FILES.name)
    {
        return std::make_pair(TABLE_FILES.id, TABLE_FILES.columns);
    }
    if (name == TABLE_TABLES.name)
    {
        return std::make_pair(TABLE_TABLES.id, TABLE_TABLES.columns);
    }
    if (name == TABLE_COLUMNS.name)
    {
        return std::make_pair(TABLE_COLUMNS.id, TABLE_COLUMNS.columns);
    }
    const std::optional<TableId> table_id = read_table(name);
    if (!table_id)
    {
        return std::nullopt;
    }
    return std::make_pair(*table_id, read_columns(*table_id));
}

std::pair<TableId, Type> get_table(const SourceText& name)
{
    auto table = find_table(name.get());
    if (!table)
    {
        throw ClientError{"table does not exist", name};
    }
    return std::move(*table);
}

std::pair<TableId, NamedColumns> get_table_named(const SourceText& name)
{
    auto table = find_table_named(name.get());
    if (!table)
    {
        throw ClientError{"table does not exist", name};
    }
    return std::move(*table);
}

FileIds get_table_file_ids(TableId table_id)
{
    // if (table_id == TABLE_STATS.id) return TABLE_STATS.file_ids;
    if (table_id == TABLE_FILES.id)
    {
        return TABLE_FILES.file_ids;
    }
    if (table_id == TABLE_TABLES.id)
    {
        return TABLE_TABLES.file_ids;
    }
    if (table_id == TABLE_COLUMNS.id)
    {
        return TABLE_COLUMNS.file_ids;
    }
    return read_table(table_id);
}

static void create_table_files(const Table& table)
{
    // data file
    os::file_create(table.getDataFileName());

    // fst file
    os::file_create(table.getFstFileName());
    fst::init(table.file_ids.fst);
}

static void register_table(const Table& table)
{
    // TABLE_FILES
    write_file(table.file_ids.dat, table.getFstFileName());
    write_file(table.file_ids.fst, table.getDataFileName());

    // TABLE_TABLES
    write_table(table.id, table.name, table.file_ids);

    // TABLE_COLUMNS
    write_columns(table.id, table.columns);
}

void init()
{
    // TODO: update statement needed
    ASSERT(system("rm -rf data") == 0);
    ASSERT(system("mkdir -p data") == 0);

    // create_table_files(TABLE_STATS);
    create_table_files(TABLE_FILES);
    create_table_files(TABLE_TABLES);
    create_table_files(TABLE_COLUMNS);

    // register_table(TABLE_STATS);
    register_table(TABLE_FILES);
    register_table(TABLE_TABLES);
    register_table(TABLE_COLUMNS);
}

static std::pair<TableId, FileIds> generate_table_ids()
{
    // TODO: update statement needed
    static TableId TABLE_ID_TODO = TableId{4};
    static FileId  FILE_ID_TODO  = FileId{8};  // NOLINT(readability-magic-numbers)
    return std::make_pair(TABLE_ID_TODO++, FileIds{FILE_ID_TODO++, FILE_ID_TODO++});
}

void create_table(std::string name, NamedColumns columns)
{
    const auto [id, file_ids] = generate_table_ids();
    const Table table         = {
                .id       = id,
                .name     = std::move(name),
                .file_ids = file_ids,
                .columns  = std::move(columns),
    };
    register_table(table);
    create_table_files(table);
}
}  // namespace catalog
