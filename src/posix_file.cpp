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
    fd_ = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (fd_ < 0)
    {
        throw ServerError{"open", path, errno};
    }
}

PosixFile::~PosixFile()
{
    if (fd_ != -1)
    {
        [[maybe_unused]] const auto err = ::close(fd_);
        assert(err == 0);
    }
}

PosixFile::PosixFile(PosixFile&& other) noexcept
{
    fd_       = other.fd_;
    other.fd_ = -1;
}

PosixFile& PosixFile::operator=(PosixFile&& other) noexcept
{
    if (this != &other)
    {
        if (fd_ != -1)
        {
            [[maybe_unused]] const auto err = ::close(fd_);
            assert(err == 0);
        }
        fd_       = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

[[nodiscard]] page::Id PosixFile::GetPageCount()
{
    assert(fd_ != -1);
    struct stat stat;
    const auto  err = ::fstat(fd_, &stat);
    if (err != 0)
    {
        throw ServerError{"fstat", errno};
    }
    if (stat.st_size % page::kSize != 0)
    {
        throw ServerError{"invalid file size"};
    }
    return static_cast<page::Id>(stat.st_size / page::kSize);
}

void PosixFile::ReadPage(page::Id id, std::span<U8, page::kSize> page)
{
    assert(fd_ != -1);
    const auto bytes = ::pread(fd_, page.data(), page::kSize, GetPageOffset(id));
    if (bytes < 0 || static_cast<page::Offset>(bytes) != page::kSize)
    {
        throw ServerError{"read", errno};
    }
}

void PosixFile::WritePage(page::Id id, std::span<const U8, page::kSize> page)
{
    assert(fd_ != -1);
    const auto bytes = ::pwrite(fd_, page.data(), page::kSize, GetPageOffset(id));
    if (bytes < 0 || static_cast<page::Offset>(bytes) != page::kSize)
    {
        throw ServerError{"write", errno};
    }
}

[[nodiscard]] page::Id PosixFile::AppendPage()
{
    assert(fd_ != -1);
    const auto id = GetPageCount();
    Resize(id + 1);
    return id;
}

void PosixFile::Truncate(page::Id new_page_count)
{
    assert(fd_ != -1);
    Resize(new_page_count);
}

[[nodiscard]] constexpr off_t PosixFile::GetPageOffset(page::Id id)
{
    return static_cast<off_t>(id.Get()) * page::kSize;
}

void PosixFile::Resize(page::Id new_page_count) const
{
    assert(fd_ != -1);
    const auto err = ::ftruncate(fd_, GetPageOffset(new_page_count));
    if (err != 0)
    {
        throw ServerError{"ftruncate", errno};
    }
}
