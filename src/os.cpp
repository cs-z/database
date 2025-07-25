#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/random.h>
#include <unistd.h>

#include "os.hpp"
#include "error.hpp"

namespace os
{
	Fd file_open(const std::string &path)
	{
		ASSERT(file_exists(path));
		const int fd = ::open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			throw ServerError { "open", path, errno };
		}
		return Fd { fd };
	}

	void file_close(Fd fd)
	{
		const int err = ::close(fd.get());
		if (err < 0) {
			throw ServerError { "close", std::to_string(fd.get()), err };
		}
	}

	bool file_exists(const std::string &path)
	{
		struct stat stat = {};
		const int err = ::stat(path.c_str(), &stat);
		if (err < 0 && errno != ENOENT) {
			throw ServerError { "stat", path, err };
		}
		return err == 0;
	}

	void file_create(const std::string &path)
	{
		ASSERT(!file_exists(path));
		const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			throw ServerError { "open", path, errno };
		}
		file_close(Fd { fd });
	}

	void file_remove(const std::string &path)
	{
		ASSERT(file_exists(path));
		if (::unlink(path.c_str()) < 0) {
			throw ServerError { "unlink", path, errno };
		}
	}

	Fd file_create_temp(const std::string &path)
	{
		const int fd = ::open(path.c_str(), O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			throw ServerError { "open", path, errno };
		}
		return Fd { fd };
	}

	void file_remove_temp(Fd fd)
	{
		file_close(fd);
	}

	static void file_seek(Fd fd, page::Id page_id)
	{
		const off_t offset = page_id.get() * page::SIZE.get();
		if (::lseek(fd.get(), offset, SEEK_SET) != offset) {
			throw ServerError { "lseek", std::to_string(fd.get()), errno };
		}
	}

	void file_read(Fd fd, page::Id page_id, void *buffer)
	{
		file_seek(fd, page_id);
		const size_t bytes = page::SIZE.get();
		const ssize_t bytes_returned = ::read(fd.get(), buffer, bytes);
		if (bytes_returned < 0) {
			throw ServerError { "read", std::to_string(fd.get()), errno };
		}
		if (static_cast<size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in read(" + std::to_string(fd.get()) + "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
	}

	void file_write(Fd fd, page::Id page_id, const void *buffer)
	{
		file_seek(fd, page_id);
		const size_t bytes = page::SIZE.get();
		const ssize_t bytes_returned = ::write(fd.get(), buffer, bytes);
		if (bytes_returned < 0) {
			throw ServerError { "write", std::to_string(fd.get()), errno };
		}
		if (static_cast<size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in write(" + std::to_string(fd.get()) + "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
	}

	unsigned int random()
	{
		unsigned int value = {};
		const size_t bytes = sizeof(value);
		const ssize_t bytes_returned = ::getrandom(&value, bytes, 0);
		if (bytes_returned < 0) {
			throw ServerError { "getrandom", errno };
		}
		if (static_cast<size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in getrandom(): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
		return value;
	}
}
