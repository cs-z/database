#include "os.hpp"
#include "common.hpp"
#include "error.hpp"
#include "page.hpp"

#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <string>

namespace os
{
static const std::string kDataDir = "data/";

static int FileOpen(const std::string& name)
{
    ASSERT(FileExists(name));
    const std::string path = kDataDir + name;
    const int         fd   = ::open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    return fd;
}

static void FileClose(int fd) noexcept
{
    const int err = ::close(fd);
    if (err < 0)
    {
        // TODO
        ASSERT(false);
        // throw ServerError { "close", std::to_string(fd), err };
    }
}

static void FileSeek(int fd, page::Id page_id)
{
    const auto offset = static_cast<off_t>(page_id.Get()) * page::kSize;
    if (::lseek(fd, offset, SEEK_SET) != offset)
    {
        throw ServerError{"lseek", std::to_string(fd), errno};
    }
}

static void FileRead(int fd, page::Id page_id, void* buffer)
{
    FileSeek(fd, page_id);
    const std::size_t bytes          = page::kSize;
    const ssize_t     bytes_returned = ::read(fd, buffer, bytes);
    if (bytes_returned < 0)
    {
        throw ServerError{"read", std::to_string(fd), errno};
    }
    if (static_cast<std::size_t>(bytes_returned) < bytes)
    {
        throw ServerError{"less bytes returned in read(" + std::to_string(fd) +
                          "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes)};
    }
}

static void FileWrite(int fd, page::Id page_id, const void* buffer)
{
    FileSeek(fd, page_id);
    const std::size_t bytes          = page::kSize;
    const ssize_t     bytes_returned = ::write(fd, buffer, bytes);
    if (bytes_returned < 0)
    {
        throw ServerError{"write", std::to_string(fd), errno};
    }
    if (static_cast<std::size_t>(bytes_returned) < bytes)
    {
        throw ServerError{"less bytes returned in write(" + std::to_string(fd) +
                          "): " + std::to_string(bytes_returned) + " < " + std::to_string(bytes)};
    }
}

static int FileCreateTemp()
{
    const std::string path = kDataDir;
    const int         fd   = ::open(path.c_str(), O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    return fd;
}

static void FileRemoveTemp(int fd) noexcept
{
    FileClose(fd);
}

bool FileExists(const std::string& name)
{
    const std::string path = kDataDir + name;
    // std::cout << "?: " << path << "\n";
    struct stat stat = {};
    const int   err  = ::stat(path.c_str(), &stat);
    if (err < 0 && errno != ENOENT)
    {
        throw ServerError{"stat", path, err};
    }
    return err == 0;
}

void FileCreate(const std::string& name)
{
    ASSERT(!FileExists(name));
    const std::string path = kDataDir + name;
    const int         fd   = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    FileClose(fd); // TODO: use it
}

void FileRemove(const std::string& name)
{
    ASSERT(FileExists(name));
    const std::string path = kDataDir + name;
    if (::unlink(path.c_str()) < 0)
    {
        throw ServerError{"unlink", path, errno};
    }
}

void FileTruncate(const std::string& name)
{
    ASSERT(FileExists(name));
    const std::string path = kDataDir + name;
    if (::truncate(path.c_str(), 0) != 0)
    {
        throw ServerError{"truncate", path, errno};
    }
}

File::File(const std::string& name) : fd_{FileOpen(name)}
{
}

File::~File() noexcept
{
    FileClose(fd_);
}

void File::Read(page::Id page_id, void* buffer) const
{
    FileRead(fd_, page_id, buffer);
}

void File::Write(page::Id page_id, const void* buffer) const
{
    FileWrite(fd_, page_id, buffer);
}

TempFile::TempFile() : fd_{FileCreateTemp()}
{
}

TempFile::~TempFile() noexcept
{
    if (fd_)
    {
        FileRemoveTemp(*fd_);
    }
}

void TempFile::Read(page::Id page_id, void* buffer) const
{
    ASSERT(fd_);
    FileRead(*fd_, page_id, buffer);
}

void TempFile::Write(page::Id page_id, const void* buffer) const
{
    ASSERT(fd_);
    FileWrite(*fd_, page_id, buffer);
}

unsigned int Random()
{
    unsigned int      value          = {};
    const std::size_t bytes          = sizeof(value);
    const ssize_t     bytes_returned = ::getrandom(&value, bytes, 0);
    if (bytes_returned < 0)
    {
        throw ServerError{"getrandom", errno};
    }
    if (static_cast<std::size_t>(bytes_returned) < bytes)
    {
        throw ServerError{"less bytes returned in getrandom(): " + std::to_string(bytes_returned) +
                          " < " + std::to_string(bytes)};
    }
    return value;
}
} // namespace os
