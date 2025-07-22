#pragma once

#include "type.hpp"
// #include "file.hpp"

namespace catalog
{
	void init();

	void insert_table(const std::string &name, std::unique_ptr<Type> type);
	void delete_table(const std::string &name);

	const Type *find_table(const std::string &name);
	const Type &get_table(const std::string &name);

	// std::pair<file::Id, file::Id> get_table_files(const std::string &name);
}
