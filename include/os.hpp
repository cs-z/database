#pragma once

#include "common.hpp"
#include "page.hpp"

namespace os
{
	bool file_exists(const std::string &name);
	void file_create(const std::string &name);
	void file_remove(const std::string &name);

	class File
	{
	public:

		File(const std::string &name);
		~File() noexcept;

		File(const File &) = delete;
		File& operator=(const File &) = delete;

		File(File &&) = delete;
		File& operator=(File &&) = delete;

		void read(page::Id page_id, void *buffer) const;
		void write(page::Id page_id, const void *buffer) const;

	private:

		const int fd;

	};

	class TempFile
	{
	public:

		TempFile();
		~TempFile() noexcept;

		TempFile(TempFile &&other) noexcept
		{
			fd = other.fd;
			other.fd = std::nullopt;
		}

		TempFile &operator=(TempFile &&other) noexcept
		{
			ASSERT(!fd); // TODO
			fd = other.fd;
			other.fd = std::nullopt;
			return *this;
		}

		TempFile(const TempFile &) = delete;
		TempFile &operator=(const TempFile &) = delete;

		void read(page::Id page_id, void *buffer) const;
		void write(page::Id page_id, const void *buffer) const;

	private:

		std::optional<int> fd;
	};

	unsigned int random();
}
