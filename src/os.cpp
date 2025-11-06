#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/random.h>
#include <unistd.h>

#include "os.hpp"
#include "error.hpp"

namespace os
{
	static const std::string DATA_DIR = "data/";

	Fd file_open(const std::string &name)
	{
		ASSERT(file_exists(name));
		const std::string path = DATA_DIR + name;
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
			throw ServerError { "close", fd.to_string(), err };
		}
	}

	bool file_exists(const std::string &name)
	{
		const std::string path = DATA_DIR + name;
		struct stat stat = {};
		const int err = ::stat(path.c_str(), &stat);
		if (err < 0 && errno != ENOENT) {
			throw ServerError { "stat", path, err };
		}
		return err == 0;
	}

	void file_create(const std::string &name)
	{
		ASSERT(!file_exists(name));
		const std::string path = DATA_DIR + name;
		const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			throw ServerError { "open", path, errno };
		}
		file_close(Fd { fd });
	}

	void file_remove(const std::string &name)
	{
		ASSERT(file_exists(name));
		const std::string path = DATA_DIR + name;
		if (::unlink(path.c_str()) < 0) {
			throw ServerError { "unlink", path, errno };
		}
	}

	static void file_seek(Fd fd, page::Id page_id)
	{
		const off_t offset = page_id.get() * page::SIZE;
		if (::lseek(fd.get(), offset, SEEK_SET) != offset) {
			throw ServerError { "lseek", fd.to_string(), errno };
		}
	}

	void file_read(Fd fd, page::Id page_id, void *buffer)
	{
		file_seek(fd, page_id);
		const std::size_t bytes = page::SIZE;
		const ssize_t bytes_returned = ::read(fd.get(), buffer, bytes);
		if (bytes_returned < 0) {
			throw ServerError { "read", fd.to_string(), errno };
		}
		if (static_cast<std::size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in read(" + fd.to_string() + "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
	}

	void file_write(Fd fd, page::Id page_id, const void *buffer)
	{
		file_seek(fd, page_id);
		const std::size_t bytes = page::SIZE;
		const ssize_t bytes_returned = ::write(fd.get(), buffer, bytes);
		if (bytes_returned < 0) {
			throw ServerError { "write", fd.to_string(), errno };
		}
		if (static_cast<std::size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in write(" + fd.to_string() + "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
	}

	static Fd file_create_temp()
	{
		const std::string path = DATA_DIR;
		const int fd = ::open(path.c_str(), O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			throw ServerError { "open", path, errno };
		}
		return Fd { fd };
	}

	static void file_remove_temp(Fd fd)
	{
		file_close(fd);
	}

	TempFile::TempFile()
	{
		fd = file_create_temp();
	}

	void TempFile::release()
	{
		if (fd) {
			file_remove_temp(*fd);
		}
	}

	unsigned int random()
	{
		unsigned int value = {};
		const std::size_t bytes = sizeof(value);
		const ssize_t bytes_returned = ::getrandom(&value, bytes, 0);
		if (bytes_returned < 0) {
			throw ServerError { "getrandom", errno };
		}
		if (static_cast<std::size_t>(bytes_returned) < bytes) {
			throw ServerError { "less bytes returned in getrandom(): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes) };
		}
		return value;
	}
}
