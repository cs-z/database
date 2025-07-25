#include "catalog.hpp"
#include "execute.hpp"
#include "fst.hpp"

namespace catalog
{
	static const std::string DATA_DIR = "data/";

	TempFile::TempFile()
	{
		// TODO: maybe add to catalog
		fd = os::file_create_temp(DATA_DIR);
	}

	TempFile::TempFile(TempFile &&other)
	{
		fd = other.fd;
		other.fd = std::nullopt;
	}

	TempFile &TempFile::operator=(TempFile &&other)
	{
		release();
		fd = other.fd;
		other.fd = std::nullopt;
		return *this;
	}

	TempFile::~TempFile()
	{
		release();
	}

	void TempFile::release()
	{
		if (fd) {
			os::file_remove_temp(*fd);
		}
	}

	static const std::string SYS_STATS = "SYS_STATS";
	static const TableId SYS_STATS_ID = TableId { 0 };
	static const TableFiles SYS_STATS_FILES = { FileId { 0 }, FileId { 1 } };
	static const TableDef SYS_STATS_DEF =
	{
		{ "FILE_COUNTER", ColumnType::INTEGER },
		{ "TABLE_COUNTER", ColumnType::INTEGER },
	};

	static const std::string SYS_FILES = "SYS_FILES";
	static const TableId SYS_FILES_ID = TableId { 1 };
	static const TableFiles SYS_FILES_FILES = { FileId { 2 }, FileId { 3 } };
	static const TableDef SYS_FILES_DEF =
	{
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
	};

	static const std::string SYS_TABLES = "SYS_TABLES";
	static const TableId SYS_TABLES_ID = TableId { 2 };
	static const TableFiles SYS_TABLES_FILES = { FileId { 4 }, FileId { 5 } };
	static const TableDef SYS_TABLES_DEF =
	{
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
		{ "FILE_FST_ID", ColumnType::INTEGER },
		{ "FILE_DAT_ID", ColumnType::INTEGER },
	};

	static const std::string SYS_COLUMNS = "SYS_COLUMNS";
	static const TableId SYS_COLUMNS_ID = TableId { 3 };
	static const TableFiles SYS_COLUMNS_FILES = { FileId { 6 }, FileId { 7 } };
	static const TableDef SYS_COLUMNS_DEF =
	{
		{ "TABLE_ID", ColumnType::INTEGER },
		{ "ID", ColumnType::INTEGER },
		{ "NAME", ColumnType::VARCHAR },
		{ "TYPE", ColumnType::VARCHAR },
	};

	static void create_table_files(TableFiles files, const std::string &table_name, bool insert);

	void init()
	{
		ASSERT(system("rm -f data/*") == 0); // TODO
		create_table_files(SYS_STATS_FILES, SYS_STATS, false);
		create_table_files(SYS_FILES_FILES, SYS_FILES, false);
		create_table_files(SYS_TABLES_FILES, SYS_TABLES, false);
		create_table_files(SYS_COLUMNS_FILES, SYS_COLUMNS, false);
	}

	static std::pair<TableId, TableFiles> create_table_ids()
	{
		static TableId TABLE_ID_TODO = TableId { 4 };
		static FileId FILE_ID_TODO = FileId { 8 };
		return std::make_pair(TABLE_ID_TODO++, TableFiles { FILE_ID_TODO++, FILE_ID_TODO++ });
	}

	static std::string read_file(FileId id)
	{
		const std::string select = "SELECT NAME FROM " + SYS_FILES + " WHERE ID = " + std::to_string(id.get());
		Value value = execute_query_internal_single(select);
		ColumnValueVarchar &name = std::get<ColumnValueVarchar>(value.at(0));
		return name;
	}

	static void write_file(FileId id, std::string name)
	{
		Value value;
		value.push_back(ColumnValueInteger { id.get() });
		value.push_back(ColumnValueVarchar { std::move(name) });
		execute_insert(SYS_FILES, value);
	}

	static std::optional<TableId> read_table(const std::string &name)
	{
		const std::string select = "SELECT ID FROM " + SYS_TABLES + " WHERE NAME = \'" + name + "\'";
		std::optional<Value> value = execute_query_internal_opt(select);
		if (!value) {
			return std::nullopt;
		}
		const ColumnValueInteger id = std::get<ColumnValueInteger>(value->at(0));
		return static_cast<TableId>(id);
	}

	static TableFiles read_table(TableId table_id)
	{
		const std::string select = "SELECT FILE_FST_ID, FILE_DAT_ID FROM " + SYS_TABLES + " WHERE ID = " + std::to_string(table_id.get());
		Value value = execute_query_internal_single(select);
		ColumnValueInteger file_fst = std::get<ColumnValueInteger>(value.at(0));
		ColumnValueInteger file_dat = std::get<ColumnValueInteger>(value.at(1));
		return { static_cast<FileId>(file_fst), static_cast<FileId>(file_dat) };
	}

	static void write_table(TableId table_id, std::string name, TableFiles files)
	{
		Value value;
		value.push_back(ColumnValueInteger { table_id.get() });
		value.push_back(ColumnValueVarchar { std::move(name) });
		value.push_back(ColumnValueInteger { files.file_fst.get() });
		value.push_back(ColumnValueInteger { files.file_dat.get() });
		execute_insert(SYS_TABLES, value);
	}

	static TableDef read_columns(TableId table_id)
	{
		const std::string select = "SELECT NAME, TYPE FROM " + SYS_COLUMNS + " WHERE TABLE_ID = " + std::to_string(table_id.get()) + " ORDER BY ID";
		std::vector<Value> values = execute_query_internal_multiple(select);
		TableDef table_def;
		for (Value &value : values) {
			ColumnValueVarchar &column_name = std::get<ColumnValueVarchar>(value.at(0));
			ColumnValueVarchar &column_type = std::get<ColumnValueVarchar>(value.at(1));
			table_def.push_back({ std::move(column_name), column_type_from_catalog_string(std::move(column_type)) });
		}
		return table_def;
	}

	static void write_columns(TableId table_id, TableDef table_def)
	{
		for (row::ColumnId column_id {}; column_id < table_def.size(); column_id++) {
			const auto &[column_name, column_type] = table_def[column_id.get()];
			Value value;
			value.push_back(ColumnValueInteger { table_id.get() });
			value.push_back(ColumnValueInteger { column_id.get() });
			value.push_back(ColumnValueVarchar { std::move(column_name) });
			value.push_back(ColumnValueVarchar { column_type_to_catalog_string(column_type) });
			execute_insert(SYS_COLUMNS, value);
		}
	}

	// TODO: cache

	std::string get_file_path(catalog::FileId file)
	{
		if (file == SYS_STATS_FILES.file_fst) return DATA_DIR + SYS_STATS + "." + "FST";
		if (file == SYS_STATS_FILES.file_dat) return DATA_DIR + SYS_STATS + "." + "DAT";
		if (file == SYS_FILES_FILES.file_fst) return DATA_DIR + SYS_FILES + "." + "FST";
		if (file == SYS_FILES_FILES.file_dat) return DATA_DIR + SYS_FILES + "." + "DAT";
		if (file == SYS_TABLES_FILES.file_fst) return DATA_DIR + SYS_TABLES + "." + "FST";
		if (file == SYS_TABLES_FILES.file_dat) return DATA_DIR + SYS_TABLES + "." + "DAT";
		if (file == SYS_COLUMNS_FILES.file_fst) return DATA_DIR + SYS_COLUMNS + "." + "FST";
		if (file == SYS_COLUMNS_FILES.file_dat) return DATA_DIR + SYS_COLUMNS + "." + "DAT";
		return DATA_DIR + read_file(file);
	}

	std::optional<std::pair<TableId, TableDef>> find_table(const std::string &table_name)
	{
		if (table_name == SYS_STATS) {
			return std::make_pair(SYS_STATS_ID, SYS_STATS_DEF);
		}
		if (table_name == SYS_FILES) {
			return std::make_pair(SYS_FILES_ID, SYS_FILES_DEF);
		}
		if (table_name == SYS_TABLES) {
			return std::make_pair(SYS_TABLES_ID, SYS_TABLES_DEF);
		}
		if (table_name == SYS_COLUMNS) {
			return std::make_pair(SYS_COLUMNS_ID, SYS_COLUMNS_DEF);
		}
		const std::optional<TableId> table_id = read_table(table_name);
		if (!table_id) {
			return std::nullopt;
		}
		return std::make_pair(*table_id, read_columns(*table_id));
	}

	TableFiles get_table_files(TableId table_id)
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
		os::file_create(DATA_DIR + name);
		if (insert) {
			write_file(file, std::move(name));
		}
	}

	static void create_table_files(TableFiles files, const std::string &table_name, bool insert)
	{
		create_file(files.file_fst, table_name, "FST", insert);
		create_file(files.file_dat, table_name, "DAT", insert);
		fst_init(files.file_fst);
	}

	void create_table(std::string table_name, TableDef table_def)
	{
		const auto [id, files] = create_table_ids();
		create_table_files(files, table_name, true);
		write_table(id, std::move(table_name), files);
		write_columns(id, std::move(table_def));
	}
}
