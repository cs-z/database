#pragma once

#include "common.hpp"
#include "page.hpp"

#include <optional>
#include <string>

namespace os
{
[[nodiscard]] bool FileExists(const std::string& name);
void               FileCreate(const std::string& name);
void               FileRemove(const std::string& name);
void               FileTruncate(const std::string& name);

class File
{
public:
    explicit File(const std::string& name);
    ~File() noexcept;

    File(const File&)            = delete;
    File& operator=(const File&) = delete;

    File(File&&)            = delete;
    File& operator=(File&&) = delete;

    void Read(page::Id page_id, void* buffer) const;
    void Write(page::Id page_id, const void* buffer) const;

private:
    const int fd_;
};

class TempFile
{
public:
    TempFile();
    ~TempFile() noexcept;

    TempFile(TempFile&& other) noexcept : fd_{other.fd_}
    {
        other.fd_ = std::nullopt;
    }

    TempFile& operator=(TempFile&& other) noexcept
    {
        ASSERT(!fd_); // TODO
        fd_       = other.fd_;
        other.fd_ = std::nullopt;
        return *this;
    }

    TempFile(const TempFile&)            = delete;
    TempFile& operator=(const TempFile&) = delete;

    void Read(page::Id page_id, void* buffer) const;
    void Write(page::Id page_id, const void* buffer) const;

private:
    std::optional<int> fd_;
};

[[nodiscard]] unsigned int Random();
} // namespace os
