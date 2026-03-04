#include "file.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <sys/types.h>

class PosixFile final : public File
{
public:
    enum class Mode : std::uint8_t
    {
        Open,
        Create,
        CreateTemp,
    };

    explicit PosixFile(const std::filesystem::path& path, Mode mode);
    ~PosixFile() override;

    PosixFile(PosixFile&& other) noexcept;
    PosixFile& operator=(PosixFile&& other) noexcept;

    PosixFile(const PosixFile&)            = delete;
    PosixFile& operator=(const PosixFile&) = delete;

    [[nodiscard]] page::Id GetPageCount() override;
    void                   ReadPage(page::Id id, std::span<U8, page::kSize> page) override;
    void                   WritePage(page::Id id, std::span<const U8, page::kSize> page) override;
    [[nodiscard]] page::Id AppendPage() override;
    void                   Truncate(page::Id new_page_count) override;

private:
    [[nodiscard]] static constexpr off_t GetPageOffset(page::Id id);
    void                                 Resize(page::Id new_page_count) const;

    int fd_ = -1; // TODO: optional int
};
