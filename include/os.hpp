#pragma once

#include "common.hpp"

namespace os
{
	struct FdTag {};
	using Fd = StrongId<FdTag, int>;

	bool file_exists(const std::string &name);

	void file_create(const std::string &name);
	void file_remove(const std::string &name);

	Fd file_open(const std::string &name);
	void file_close(Fd fd);

	void file_read(Fd fd, PageId page_id, void *buffer);
	void file_write(Fd fd, PageId page_id, const void *buffer);

	class TempFile
	{
	public:

		TempFile();

		TempFile(TempFile &&other)
		{
			fd = other.fd;
			other.fd = std::nullopt;
		}

		TempFile &operator=(TempFile &&other)
		{
			release();
			fd = other.fd;
			other.fd = std::nullopt;
			return *this;
		}

		TempFile(const TempFile &) = delete;
		TempFile &operator=(const TempFile &) = delete;

		~TempFile()
		{
			release();
		}

		inline os::Fd get() const { return fd.value(); }

	private:

		void release();

		std::optional<os::Fd> fd;
	};

	unsigned int random();
}
