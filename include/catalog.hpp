#pragma once

#include "type.hpp"
#include "os.hpp"
#include "row.hpp"

namespace catalog
{
	struct FileTag {};
	using FileId = StrongId<FileTag, unsigned int>;

	struct TableTag {};
	using TableId = StrongId<TableTag, unsigned int>;

	class TempFile
	{
	public:

		TempFile();

		TempFile(TempFile &&other);
		TempFile &operator=(TempFile &&other);

		TempFile(const TempFile &) = delete;
		TempFile &operator=(const TempFile &) = delete;

		~TempFile();

		inline os::Fd get() const
		{
			return fd.value();
		}

	private:

		void release();

		std::optional<os::Fd> fd;
	};

	struct TableFiles
	{
		FileId file_fst;
		FileId file_dat;
	};

	void init();

	struct ColumnDef
	{
		std::string name;
		ColumnType type;
	};

	using TableDef = std::vector<ColumnDef>;

	std::string get_file_path(catalog::FileId file);

	std::optional<std::pair<TableId, TableDef>> find_table(const std::string &table_name);
	TableFiles get_table_files(TableId table_id);

	void create_table(std::string table_name, TableDef table_def);
	void remove_table(TableId table_id);
}
