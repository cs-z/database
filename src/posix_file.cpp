#include "posix_file.hpp"
#include "common.hpp"
#include "error.hpp"
#include "page.hpp"

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <span>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

PosixFile::PosixFile(const std::filesystem::path& path, Mode mode)
{
    int flags = 0;
    switch (mode)
    {
    case Mode::Open:
        flags = O_RDWR;
        break;
    case Mode::Create:
        flags = O_RDWR | O_CREAT | O_EXCL;
        break;
    case Mode::CreateTemp:
        flags = O_RDWR | O_TMPFILE;
        break;
    }
    fd = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        throw ServerError{"open", path, errno};
    }
}

PosixFile::~PosixFile()
{
    if (fd != -1)
    {
        [[maybe_unused]] const auto err = ::close(fd);
        assert(err == 0);
    }
}

PosixFile::PosixFile(PosixFile&& other) noexcept
{
    fd       = other.fd;
    other.fd = -1;
}

PosixFile& PosixFile::operator=(PosixFile&& other) noexcept
{
    if (this != &other)
    {
        if (fd != -1)
        {
            [[maybe_unused]] const auto err = ::close(fd);
            assert(err == 0);
        }
        fd       = other.fd;
        other.fd = -1;
    }
    return *this;
}

[[nodiscard]] page::Id PosixFile::getPageCount()
{
    assert(fd != -1);
    struct stat stat;
    const auto  err = ::fstat(fd, &stat);
    if (err != 0)
    {
        throw ServerError{"fstat", errno};
    }
    if (stat.st_size % page::SIZE != 0)
    {
        throw ServerError{"invalid file size"};
    }
    return static_cast<page::Id>(stat.st_size / page::SIZE);
}

void PosixFile::readPage(page::Id id, std::span<u8, page::SIZE> page)
{
    assert(fd != -1);
    const auto bytes = ::pread(fd, page.data(), page::SIZE, getPageOffset(id));
    if (bytes < 0 || static_cast<page::Offset>(bytes) != page::SIZE)
    {
        throw ServerError{"read", errno};
    }
}

void PosixFile::writePage(page::Id id, std::span<const u8, page::SIZE> page)
{
    assert(fd != -1);
    const auto bytes = ::pwrite(fd, page.data(), page::SIZE, getPageOffset(id));
    if (bytes < 0 || static_cast<page::Offset>(bytes) != page::SIZE)
    {
        throw ServerError{"write", errno};
    }
}

[[nodiscard]] page::Id PosixFile::appendPage()
{
    assert(fd != -1);
    const auto id = getPageCount();
    resize(id + 1);
    return id;
}

void PosixFile::truncate(page::Id newPageCount)
{
    assert(fd != -1);
    resize(newPageCount);
}

[[nodiscard]] constexpr off_t PosixFile::getPageOffset(page::Id id)
{
    return static_cast<off_t>(id.get()) * page::SIZE;
}

void PosixFile::resize(page::Id newPageCount) const
{
    assert(fd != -1);
    const auto err = ::ftruncate(fd, getPageOffset(newPageCount));
    if (err != 0)
    {
        throw ServerError{"ftruncate", errno};
    }
}
