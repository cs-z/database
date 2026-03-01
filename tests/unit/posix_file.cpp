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
        testDir         = std::filesystem::temp_directory_path();
        testFilePath    = testDir / "database_test_file.dat";
        invalidFilePath = testDir / "database_test_file_should_not_exist.dat";
        removeFile();
    }

    void TearDown() override
    {
        removeFile();
    }

    void removeFile() const
    {
        if (std::filesystem::exists(testFilePath))
        {
            std::filesystem::remove(testFilePath);
        }
    }

    std::filesystem::path testDir;
    std::filesystem::path testFilePath;
    std::filesystem::path invalidFilePath;
};

TEST_F(PosixFileTest, CreateAndReopen)
{
    {
        PosixFile file{testFilePath, PosixFile::Mode::Create};
        EXPECT_EQ(file.getPageCount(), 0);
        EXPECT_EQ(file.appendPage(), 0);
        EXPECT_EQ(file.getPageCount(), 1);
    }
    PosixFile file{testFilePath, PosixFile::Mode::Open};
    EXPECT_EQ(file.getPageCount(), 1);
}

TEST_F(PosixFileTest, CreateTemp)
{
    PosixFile file{testDir, PosixFile::Mode::CreateTemp};
    EXPECT_EQ(file.getPageCount(), 0);
    EXPECT_EQ(file.appendPage(), 0);
    EXPECT_EQ(file.getPageCount(), 1);
}

TEST_F(PosixFileTest, ReadWrite)
{
    PosixFile file{testFilePath, PosixFile::Mode::Create};
    EXPECT_EQ(file.getPageCount(), 0);
    EXPECT_EQ(file.appendPage(), 0);
    EXPECT_EQ(file.getPageCount(), 1);

    static constexpr u8 kValidByte = 0xAB;
    static constexpr u8 kDirtyByte = 0xFF;

    std::array<u8, page::SIZE> writeBuffer;
    std::ranges::fill(writeBuffer, kValidByte);
    file.writePage(page::Id{0}, writeBuffer);

    std::array<u8, page::SIZE> readBuffer;
    std::ranges::fill(readBuffer, kDirtyByte);
    file.readPage(page::Id{0}, readBuffer);

    EXPECT_EQ(writeBuffer, readBuffer);
}

TEST_F(PosixFileTest, WriteMultiple)
{
    PosixFile  file{testFilePath, PosixFile::Mode::Create};
    const auto idA = file.appendPage();
    const auto idB = file.appendPage();

    std::array<u8, page::SIZE> buffer;

    static constexpr u8 kByteA = 0xAA;
    std::ranges::fill(buffer, kByteA);
    file.writePage(idA, buffer);

    static constexpr u8 kByteB = 0xBB;
    std::ranges::fill(buffer, kByteB);
    file.writePage(idB, buffer);

    file.readPage(idA, buffer);
    EXPECT_EQ(buffer.front(), kByteA);
    EXPECT_EQ(buffer.back(), kByteA);

    file.readPage(idB, buffer);
    EXPECT_EQ(buffer.front(), kByteB);
    EXPECT_EQ(buffer.back(), kByteB);
}

TEST_F(PosixFileTest, Truncate)
{
    PosixFile file{testFilePath, PosixFile::Mode::Create};
    EXPECT_EQ(file.getPageCount(), 0);

    EXPECT_EQ(file.appendPage(), 0);
    EXPECT_EQ(file.getPageCount(), 1);

    EXPECT_EQ(file.appendPage(), 1);
    EXPECT_EQ(file.getPageCount(), 2);

    file.truncate(page::Id{1});
    EXPECT_EQ(file.getPageCount(), 1);

    file.truncate(page::Id{0});
    EXPECT_EQ(file.getPageCount(), 0);
}

TEST_F(PosixFileTest, MoveSemantics)
{
    PosixFile file{testFilePath, PosixFile::Mode::Create};
    EXPECT_EQ(file.getPageCount(), 0);
    EXPECT_EQ(file.appendPage(), 0);
    EXPECT_EQ(file.getPageCount(), 1);

    PosixFile newFile = std::move(file);
    EXPECT_EQ(newFile.getPageCount(), 1);
    EXPECT_EQ(newFile.appendPage(), 1);
    EXPECT_EQ(newFile.getPageCount(), 2);
}

TEST_F(PosixFileTest, OpenNonExistingFile)
{
    EXPECT_THROW((PosixFile{invalidFilePath, PosixFile::Mode::Open}), ServerError);
}

TEST_F(PosixFileTest, ReadInvalidPage)
{
    PosixFile                  file{testFilePath, PosixFile::Mode::Create};
    std::array<u8, page::SIZE> readBuffer;
    EXPECT_THROW((file.readPage(page::Id{1}, readBuffer)), ServerError);
}
