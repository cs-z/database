#include "catalog.hpp"
#include "execute.hpp"
#include "fst.hpp"
#include "value.hpp"
#include "os.hpp"

namespace catalog
{
	static const std::string SYS_STATS = "SYS_STATS";
	static const TableId SYS_STATS_ID = TableId { 0 };
	static const std::pair<FileId, FileId> SYS_STATS_FILES = std::make_pair(FileId { 0 }, FileId { 1 });
	static const NamedColumns SYS_STATS_COLUMNS =
	{
		{ "FILE_COUNTER", ColumnType::INTEGER },
		{ "TABLE_COUNTER", ColumnType::INTEGER },
	};

	static const std::string SYS_FILES = "SYS_FILES";
	static const TableId SYS_FILES_ID = TableId { 1 };
	static const std::pair<FileId, FileId> SYS_FILES_FILES = std::make_pair(FileId { 2 }, FileId { 3 });
	static const NamedColumns SYS_FILES_COLUMNS =
	{
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
	};

	static const std::string SYS_TABLES = "SYS_TABLES";
	static const TableId SYS_TABLES_ID = TableId { 2 };
	static const std::pair<FileId, FileId> SYS_TABLES_FILES = std::make_pair(FileId { 4 }, FileId { 5 });
	static const NamedColumns SYS_TABLES_COLUMNS =
	{
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
		{ "FILE_FST_ID", ColumnType::INTEGER },
		{ "FILE_DAT_ID", ColumnType::INTEGER },
	};

	static const std::string SYS_COLUMNS = "SYS_COLUMNS";
	static const TableId SYS_COLUMNS_ID = TableId { 3 };
	static const std::pair<FileId, FileId> SYS_COLUMNS_FILES = std::make_pair(FileId { 6 }, FileId { 7 });
	static const NamedColumns SYS_COLUMNS_COLUMNS =
	{
		{ "TABLE_ID", ColumnType::INTEGER },
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
		{ "TYPE", ColumnType::VARCHAR },
	};

	static void create_table_files(std::pair<FileId, FileId> files, const std::string &table_name, bool insert);

	void init()
	{
		// TODO: update statement needed
		ASSERT(system("rm -rf data") == 0);
		ASSERT(system("mkdir -p data") == 0);

		create_table_files(SYS_STATS_FILES, SYS_STATS, false);
		create_table_files(SYS_FILES_FILES, SYS_FILES, false);
		create_table_files(SYS_TABLES_FILES, SYS_TABLES, false);
		create_table_files(SYS_COLUMNS_FILES, SYS_COLUMNS, false);
	}

	static std::pair<TableId, std::pair<FileId, FileId>> create_table_ids()
	{
		// TODO: update statement needed
		static TableId TABLE_ID_TODO = TableId { 4 };
		static FileId FILE_ID_TODO = FileId { 8 };
		return std::make_pair(TABLE_ID_TODO++, std::make_pair(FILE_ID_TODO++, FILE_ID_TODO++));
	}

	static std::string read_file(FileId id)
	{
		const std::string statement = "SELECT NAME FROM " + SYS_FILES + " WHERE ID = " + id.to_string();
		std::vector<Value> values = execute_internal_statement(statement);
		ASSERT(values.size() == 1);
		ColumnValueVarchar &name = std::get<ColumnValueVarchar>(values.front().at(0));
		return name;
	}

	static void write_file(FileId id, std::string name)
	{
		const Value value = {
			ColumnValueInteger { id.get() },
			ColumnValueVarchar { std::move(name) },
		};
		const std::string statement = "INSERT INTO " + SYS_FILES + " VALUES " + value_to_list(value);
		ASSERT(execute_internal_statement(statement).size() == 0);
	}

	static std::optional<TableId> read_table(const std::string &name)
	{
		const std::string statement = "SELECT ID FROM " + SYS_TABLES + " WHERE NAME = \'" + name + "\'";
		std::vector<Value> values = execute_internal_statement(statement);
		if (values.empty()) {
			return std::nullopt;
		}
		ASSERT(values.size() == 1);
		const ColumnValueInteger id = std::get<ColumnValueInteger>(values.front().at(0));
		return static_cast<TableId>(id);
	}

	static std::pair<FileId, FileId> read_table(TableId table_id)
	{
		const std::string statement = "SELECT FILE_FST_ID, FILE_DAT_ID FROM " + SYS_TABLES + " WHERE ID = " + table_id.to_string();
		std::vector<Value> values = execute_internal_statement(statement);
		ASSERT(values.size() == 1);
		ColumnValueInteger file_fst = std::get<ColumnValueInteger>(values.front().at(0));
		ColumnValueInteger file_dat = std::get<ColumnValueInteger>(values.front().at(1));
		return { static_cast<FileId>(file_fst), static_cast<FileId>(file_dat) };
	}

	static void write_table(TableId table_id, std::string name, std::pair<FileId, FileId> files)
	{
		const Value value = {
			ColumnValueInteger { table_id.get() },
			ColumnValueVarchar { std::move(name) },
			ColumnValueInteger { files.first.get() },
			ColumnValueInteger { files.second.get() },
		};
		const std::string statement = "INSERT INTO " + SYS_TABLES + " VALUES " + value_to_list(value);
		ASSERT(execute_internal_statement(statement).size() == 0);
	}

	static NamedColumns read_columns(TableId table_id)
	{
		const std::string statement = "SELECT NAME, TYPE FROM " + SYS_COLUMNS + " WHERE TABLE_ID = " + table_id.to_string() + " ORDER BY ID";
		std::vector<Value> values = execute_internal_statement(statement);
		ASSERT(values.size() > 0);
		NamedColumns columns;
		for (Value &value : values) {
			ColumnValueVarchar &column_name = std::get<ColumnValueVarchar>(value.at(0));
			ColumnValueVarchar &column_type = std::get<ColumnValueVarchar>(value.at(1));
			columns.emplace_back(std::move(column_name), column_type_from_catalog_string(std::move(column_type)));
		}
		return columns;
	}

	static void write_columns(TableId table_id, NamedColumns columns)
	{
		for (ColumnId column_id {}; column_id < columns.size(); column_id++) {
			const auto &[column_name, column_type] = columns[column_id.get()];
			const Value value = {
				ColumnValueInteger { table_id.get() },
				ColumnValueInteger { column_id.get() },
				ColumnValueVarchar { std::move(column_name) },
				ColumnValueVarchar { column_type_to_catalog_string(column_type) },
			};
			const std::string statement = "INSERT INTO " + SYS_COLUMNS + " VALUES " + value_to_list(value);
			ASSERT(execute_internal_statement(statement).size() == 0);
		}
	}

	// TODO: cache

	std::string get_file_name(catalog::FileId file)
	{
		if (file == SYS_STATS_FILES.first) return SYS_STATS + "." + "FST";
		if (file == SYS_STATS_FILES.second) return SYS_STATS + "." + "DAT";
		if (file == SYS_FILES_FILES.first) return SYS_FILES + "." + "FST";
		if (file == SYS_FILES_FILES.second) return SYS_FILES + "." + "DAT";
		if (file == SYS_TABLES_FILES.first) return SYS_TABLES + "." + "FST";
		if (file == SYS_TABLES_FILES.second) return SYS_TABLES + "." + "DAT";
		if (file == SYS_COLUMNS_FILES.first) return SYS_COLUMNS + "." + "FST";
		if (file == SYS_COLUMNS_FILES.second) return SYS_COLUMNS + "." + "DAT";
		return read_file(file);
	}

	std::optional<std::pair<TableId, Type>> find_table(const std::string &table_name)
	{
		// TODO: avoid copy
		auto table = find_table_named(table_name);
		if (!table) return std::nullopt;
		Type type;
		for (const auto &[column_name, column_type] : table->second) {
			type.push(column_type);
		}
		return std::make_pair(table->first, std::move(type));
	}

	std::optional<std::pair<TableId, NamedColumns>> find_table_named(const std::string &table_name)
	{
		if (table_name == SYS_STATS) {
			return std::make_pair(SYS_STATS_ID, SYS_STATS_COLUMNS);
		}
		if (table_name == SYS_FILES) {
			return std::make_pair(SYS_FILES_ID, SYS_FILES_COLUMNS);
		}
		if (table_name == SYS_TABLES) {
			return std::make_pair(SYS_TABLES_ID, SYS_TABLES_COLUMNS);
		}
		if (table_name == SYS_COLUMNS) {
			return std::make_pair(SYS_COLUMNS_ID, SYS_COLUMNS_COLUMNS);
		}
		const std::optional<TableId> table_id = read_table(table_name);
		if (!table_id) {
			return std::nullopt;
		}
		return std::make_pair(*table_id, read_columns(*table_id));
	}

	std::pair<TableId, Type> get_table(const SourceText &table_name)
	{
		auto table = find_table(table_name.get());
		if (!table) {
			throw ClientError { "table does not exist", table_name };
		}
		return std::move(*table);
	}

	std::pair<TableId, NamedColumns> get_table_named(const SourceText &table_name)
	{
		auto table = find_table_named(table_name.get());
		if (!table) {
			throw ClientError { "table does not exist", table_name };
		}
		return std::move(*table);
	}

	std::pair<FileId, FileId> get_table_files(TableId table_id)
	{
		if (table_id == SYS_STATS_ID) {
			return SYS_STATS_FILES;
		}
		if (table_id == SYS_FILES_ID) {
			return SYS_FILES_FILES;
		}
		if (table_id == SYS_TABLES_ID) {
			return SYS_TABLES_FILES;
		}
		if (table_id == SYS_COLUMNS_ID) {
			return SYS_COLUMNS_FILES;
		}
		return read_table(table_id);
	}

	static void create_file(FileId file, const std::string &table_name, const std::string &type, bool insert)
	{
		std::string name = table_name + "." + type;
		os::file_create(name);
		if (insert) {
			write_file(file, std::move(name));
		}
	}

	static void create_table_files(std::pair<FileId, FileId> files, const std::string &table_name, bool insert)
	{
		create_file(files.first, table_name, "FST", insert);
		create_file(files.second, table_name, "DAT", insert);
		fst::init(files.first);
	}

	void create_table(std::string table_name, NamedColumns columns)
	{
		const auto [id, files] = create_table_ids();
		create_table_files(files, table_name, true);
		write_table(id, std::move(table_name), files);
		write_columns(id, std::move(columns));
	}
}
