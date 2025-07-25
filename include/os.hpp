#pragma once

#include "common.hpp"
#include "page.hpp"

namespace os
{
	struct FdTag {};
	using Fd = StrongId<FdTag, int>;

	bool file_exists(const std::string &path);

	void file_create(const std::string &path);
	void file_remove(const std::string &path);

	Fd file_create_temp(const std::string &path);
	void file_remove_temp(Fd fd);

	Fd file_open(const std::string &path);
	void file_close(Fd fd);

	void file_read(Fd fd, page::Id page_id, void *buffer);
	void file_write(Fd fd, page::Id page_id, const void *buffer);

	unsigned int random();
}
