#include "posix_file.hpp"
#include "common.hpp"
#include "error.hpp"
#include "page.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>

class PosixFileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_          = std::filesystem::temp_directory_path();
        test_file_path_    = test_dir_ / "database_test_file.dat";
        invalid_file_path_ = test_dir_ / "database_test_file_should_not_exist.dat";
        RemoveFile();
    }

    void TearDown() override
    {
        RemoveFile();
    }

    void RemoveFile() const
    {
        if (std::filesystem::exists(test_file_path_))
        {
            std::filesystem::remove(test_file_path_);
        }
    }

    std::filesystem::path test_dir_;
    std::filesystem::path test_file_path_;
    std::filesystem::path invalid_file_path_;
};

TEST_F(PosixFileTest, CreateAndReopen)
{
    {
        PosixFile file{test_file_path_, PosixFile::Mode::Create};
        EXPECT_EQ(file.GetPageCount(), 0);
        EXPECT_EQ(file.AppendPage(), 0);
        EXPECT_EQ(file.GetPageCount(), 1);
    }
    PosixFile file{test_file_path_, PosixFile::Mode::Open};
    EXPECT_EQ(file.GetPageCount(), 1);
}

TEST_F(PosixFileTest, CreateTemp)
{
    PosixFile file{test_dir_, PosixFile::Mode::CreateTemp};
    EXPECT_EQ(file.GetPageCount(), 0);
    EXPECT_EQ(file.AppendPage(), 0);
    EXPECT_EQ(file.GetPageCount(), 1);
}

TEST_F(PosixFileTest, ReadWrite)
{
    PosixFile file{test_file_path_, PosixFile::Mode::Create};
    EXPECT_EQ(file.GetPageCount(), 0);
    EXPECT_EQ(file.AppendPage(), 0);
    EXPECT_EQ(file.GetPageCount(), 1);

    static constexpr U8 kValidByte = 0xAB;
    static constexpr U8 kDirtyByte = 0xFF;

    std::array<U8, page::kSize> write_buffer;
    std::ranges::fill(write_buffer, kValidByte);
    file.WritePage(page::Id{0}, write_buffer);

    std::array<U8, page::kSize> read_buffer;
    std::ranges::fill(read_buffer, kDirtyByte);
    file.ReadPage(page::Id{0}, read_buffer);

    EXPECT_EQ(write_buffer, read_buffer);
}

TEST_F(PosixFileTest, WriteMultiple)
{
    PosixFile  file{test_file_path_, PosixFile::Mode::Create};
    const auto id_a = file.AppendPage();
    const auto id_b = file.AppendPage();

    std::array<U8, page::kSize> buffer;

    static constexpr U8 kByteA = 0xAA;
    std::ranges::fill(buffer, kByteA);
    file.WritePage(id_a, buffer);

    static constexpr U8 kByteB = 0xBB;
    std::ranges::fill(buffer, kByteB);
    file.WritePage(id_b, buffer);

    file.ReadPage(id_a, buffer);
    EXPECT_EQ(buffer.front(), kByteA);
    EXPECT_EQ(buffer.back(), kByteA);

    file.ReadPage(id_b, buffer);
    EXPECT_EQ(buffer.front(), kByteB);
    EXPECT_EQ(buffer.back(), kByteB);
}

TEST_F(PosixFileTest, Truncate)
{
    PosixFile file{test_file_path_, PosixFile::Mode::Create};
    EXPECT_EQ(file.GetPageCount(), 0);

    EXPECT_EQ(file.AppendPage(), 0);
    EXPECT_EQ(file.GetPageCount(), 1);

    EXPECT_EQ(file.AppendPage(), 1);
    EXPECT_EQ(file.GetPageCount(), 2);

    file.Truncate(page::Id{1});
    EXPECT_EQ(file.GetPageCount(), 1);

    file.Truncate(page::Id{0});
    EXPECT_EQ(file.GetPageCount(), 0);
}

TEST_F(PosixFileTest, MoveSemantics)
{
    PosixFile file{test_file_path_, PosixFile::Mode::Create};
    EXPECT_EQ(file.GetPageCount(), 0);
    EXPECT_EQ(file.AppendPage(), 0);
    EXPECT_EQ(file.GetPageCount(), 1);

    PosixFile new_file = std::move(file);
    EXPECT_EQ(new_file.GetPageCount(), 1);
    EXPECT_EQ(new_file.AppendPage(), 1);
    EXPECT_EQ(new_file.GetPageCount(), 2);
}

TEST_F(PosixFileTest, OpenNonExistingFile)
{
    EXPECT_THROW((PosixFile{invalid_file_path_, PosixFile::Mode::Open}), ServerError);
}

TEST_F(PosixFileTest, ReadInvalidPage)
{
    PosixFile                   file{test_file_path_, PosixFile::Mode::Create};
    std::array<U8, page::kSize> read_buffer;
    EXPECT_THROW((file.ReadPage(page::Id{1}, read_buffer)), ServerError);
}
