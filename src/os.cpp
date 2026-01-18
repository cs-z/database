#include <sys/fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.hpp"
#include "os.hpp"

namespace os
{
static const std::string DATA_DIR = "data/";

static int file_open(const std::string& name)
{
    ASSERT(file_exists(name));
    const std::string path = DATA_DIR + name;
    const int         fd   = ::open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    ////fprintf(stderr, "fopen: %s, %d\n", name.c_str(), fd);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    return fd;
}

static void file_close(int fd) noexcept
{
    const int err = ::close(fd);
    ////fprintf(stderr, "fclose: %d\n", fd);
    if (err < 0)
    {
        // TODO
        ASSERT(false);
        // throw ServerError { "close", std::to_string(fd), err };
    }
}

static void file_seek(int fd, page::Id page_id)
{
    const auto offset = static_cast<off_t>(page_id.get()) * page::SIZE;
    // fprintf(stderr, "fseek: %d, %u\n", fd, page_id.get());
    if (::lseek(fd, offset, SEEK_SET) != offset)
    {
        throw ServerError{"lseek", std::to_string(fd), errno};
    }
}

static void file_read(int fd, page::Id page_id, void* buffer)
{
    file_seek(fd, page_id);
    const std::size_t bytes          = page::SIZE;
    const ssize_t     bytes_returned = ::read(fd, buffer, bytes);
    // fprintf(stderr, "fread: %d, %u, %p\n", fd, page_id.get(), (void*)buffer);
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

static void file_write(int fd, page::Id page_id, const void* buffer)
{
    file_seek(fd, page_id);
    const std::size_t bytes          = page::SIZE;
    const ssize_t     bytes_returned = ::write(fd, buffer, bytes);
    // fprintf(stderr, "fwrite: %d, %u, %p\n", fd, page_id.get(), (void*)buffer);
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

static int file_create_temp()
{
    const std::string path = DATA_DIR;
    const int         fd   = ::open(path.c_str(), O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
    ////fprintf(stderr, "fopen temp: %d\n", fd);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    return fd;
}

static void file_remove_temp(int fd) noexcept
{
    ////fprintf(stderr, "closing temp: ");
    file_close(fd);
}

bool file_exists(const std::string& name)
{
    const std::string path = DATA_DIR + name;
    struct stat       stat = {};
    const int         err  = ::stat(path.c_str(), &stat);
    if (err < 0 && errno != ENOENT)
    {
        throw ServerError{"stat", path, err};
    }
    return err == 0;
}

void file_create(const std::string& name)
{
    ASSERT(!file_exists(name));
    const std::string path = DATA_DIR + name;
    const int         fd   = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    ////fprintf(stderr, "fcreate: %s, %d\n", name.c_str(), fd);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
    file_close(fd);  // TODO: use it
}

void file_remove(const std::string& name)
{
    ASSERT(file_exists(name));
    const std::string path = DATA_DIR + name;
    ////fprintf(stderr, "fremove: %s\n", name.c_str());
    if (::unlink(path.c_str()) < 0)
    {
        throw ServerError{"unlink", path, errno};
    }
}

File::File(const std::string& name) : fd{file_open(name)}
{
}

File::~File() noexcept
{
    file_close(fd);
}

void File::read(page::Id page_id, void* buffer) const
{
    file_read(fd, page_id, buffer);
}

void File::write(page::Id page_id, const void* buffer) const
{
    file_write(fd, page_id, buffer);
}

TempFile::TempFile() : fd{file_create_temp()}
{
}

TempFile::~TempFile() noexcept
{
    if (fd)
    {
        file_remove_temp(*fd);
    }
}

void TempFile::read(page::Id page_id, void* buffer) const
{
    ASSERT(fd);
    file_read(*fd, page_id, buffer);
}

void TempFile::write(page::Id page_id, const void* buffer) const
{
    ASSERT(fd);
    file_write(*fd, page_id, buffer);
}

unsigned int random()
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
}  // namespace os
