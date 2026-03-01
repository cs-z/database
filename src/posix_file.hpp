#include "file.hpp"

#include <filesystem>
#include <span>
#include <sys/types.h>

class PosixFile final : public File
{
public:
    enum class Mode
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

    [[nodiscard]] page::Id getPageCount() override;
    void                   readPage(page::Id id, std::span<u8, page::SIZE> page) override;
    void                   writePage(page::Id id, std::span<const u8, page::SIZE> page) override;
    [[nodiscard]] page::Id appendPage() override;
    void                   truncate(page::Id newPageCount) override;

private:
    [[nodiscard]] static constexpr off_t getPageOffset(page::Id id);
    void                                 resize(page::Id newPageCount) const;

    int fd = -1;
};
