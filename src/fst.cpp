#include "fst.hpp"
#include "buffer.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "os.hpp"
#include "page.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

// FREE SPACE TREE: stores free space of data pages, one entry per page

namespace fst
{
constexpr page::EntryId kEntriesPerPage{page::kSize / sizeof(page::Offset) / 2};

struct PageHead
{
    static constexpr unsigned int kMaxLevel = 6; // TODO

    static constexpr std::array<page::Id, kMaxLevel + 1> kLevelPages = {
        static_cast<page::Id>(0),
        static_cast<page::Id>(1),
        static_cast<page::Id>(kEntriesPerPage.Get()),
        static_cast<page::Id>(kEntriesPerPage.Get()) * static_cast<page::Id>(kEntriesPerPage.Get()),
        static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()),
        static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()),
        static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()) *
            static_cast<page::Id>(kEntriesPerPage.Get()),
    };

    static constexpr std::array<page::Id, kMaxLevel + 1> kLevelBegin = {
        page::Id{1},
        page::Id{1} + kLevelPages[0],
        page::Id{1} + kLevelPages[0] + kLevelPages[1],
        page::Id{1} + kLevelPages[0] + kLevelPages[1] + kLevelPages[2],
        page::Id{1} + kLevelPages[0] + kLevelPages[1] + kLevelPages[2] + kLevelPages[3],
        page::Id{1} + kLevelPages[0] + kLevelPages[1] + kLevelPages[2] + kLevelPages[3] +
            kLevelPages[4],
        page::Id{1} + kLevelPages[0] + kLevelPages[1] + kLevelPages[2] + kLevelPages[3] +
            kLevelPages[4] + kLevelPages[5],
    };

    unsigned int pages;
    unsigned int levels;
    unsigned int bottom; // number of pages in the bottom level

    void Init()
    {
        pages  = 0;
        levels = 0;
        bottom = 0;
    }
};

// static void page_print(const page::Offset *page)
// {
// 	ASSERT(page);
// 	page::EntryId index { 1 };
// 	page::EntryId count { 1 };
// 	while (count <= kEntriesPerPage) {
// 		for (page::EntryId i {}; i < count; i++) {
// 			std::printf("%u ", page[(index + i).get()]);
// 		}
// 		std::printf("\n");
// 		index = index + count;
// 		count = count * 2;
// 	}
// }

static inline page::Offset PageGetRoot(const page::Offset* page)
{
    ASSERT(page);
    return page[1];
}

static void PageInit(page::Offset* page)
{
    ASSERT(page);
    memset(page, 0, page::kSize);
}

static bool PageSet(page::Offset* page, page::EntryId entry_id, page::Offset size)
{
    ASSERT(page);
    ASSERT(entry_id < kEntriesPerPage);
    page::EntryId index = kEntriesPerPage + entry_id;
    while (index > 0 && page[index.Get()] != size)
    {
        page[index.Get()] = size;
        index             = index / 2;
        size              = std::max(page[(index * 2).Get()], page[(index * 2 + 1).Get()]);
    }
    return index == 0;
}

static page::EntryId PageGet(const page::Offset* page, page::Offset size)
{
    ASSERT(page);
    ASSERT(size > 0);
    ASSERT(PageGetRoot(page) >= size);
    page::EntryId index{1};
    while (index < kEntriesPerPage)
    {
        const page::EntryId index_l = index * 2;
        const page::EntryId index_r = index * 2 + 1;
        const page::Offset  size_l  = page[index_l.Get()];
        const page::Offset  size_r  = page[index_r.Get()];
        // TODO: best fit or worst fit
        if (size_l >= size)
        {
            index = index_l;
            continue;
        }
        if (size_r >= size)
        {
            index = index_r;
            continue;
        }
        UNREACHABLE();
    }
    ASSERT(kEntriesPerPage <= index && index < 2 * kEntriesPerPage);
    ASSERT(page[index.Get()] >= size);
    return index - kEntriesPerPage;
}

void Init(catalog::FileId file_id)
{
    const buffer::Pin<PageHead> page_head{file_id, page::Id{}, true};
    page_head->Init();
}

page::Id GetPageCount(catalog::FileId file_id)
{
    const buffer::Pin<const PageHead> page_head{file_id, page::Id{}};
    return page::Id{page_head->pages};
}

static page::Id Append(catalog::FileId file_id, page::Offset value)
{
    const buffer::Pin<PageHead> page_head{file_id, page::Id{}};
    if (page_head->pages % kEntriesPerPage == 0)
    {
        if (page_head->bottom == PageHead::kLevelPages[page_head->levels])
        {
            ASSERT(page_head->levels < PageHead::kMaxLevel);
            for (page::Id page_id{}; page_id < PageHead::kLevelPages[page_head->levels]; page_id++)
            {
                const buffer::Pin<const page::Offset> src{
                    file_id, page::Id{PageHead::kLevelBegin[page_head->levels] + page_id}};
                const buffer::Pin<page::Offset> dst{
                    file_id, page::Id{PageHead::kLevelBegin[page_head->levels + 1] + page_id},
                    true};
                std::memcpy(dst.GetPage(), src.GetPage(), page::kSize);
            }
            for (unsigned int level = 1; level <= page_head->levels; level++)
            {
                for (page::Id page_id{}; page_id < PageHead::kLevelPages[level]; page_id++)
                {
                    const buffer::Pin<page::Offset> page{
                        file_id, page::Id{PageHead::kLevelBegin[level] + page_id}};
                    PageInit(page.GetPage());
                }
            }
            for (page::Id page_id{}; page_id < PageHead::kLevelPages[page_head->levels]; page_id++)
            {
                const buffer::Pin<const page::Offset> page{
                    file_id, page::Id{PageHead::kLevelBegin[page_head->levels + 1] + page_id}};
                const page::Offset value = PageGetRoot(page.GetPage());
                Update(file_id, page_id, value);
            }
            page_head->levels++;
        }
        const buffer::Pin<page::Offset> page{
            file_id, page::Id{PageHead::kLevelBegin[page_head->levels] + page_head->bottom++},
            true};
        PageInit(page.GetPage());
    }
    const page::Id page_id = page::Id{page_head->pages++};
    Update(file_id, page_id, value);
    return page_id;
}

void Update(catalog::FileId file_id, page::Id page_id, page::Offset size)
{
    const buffer::Pin<const PageHead> page_head{file_id, page::Id{0}};
    ASSERT(page_id < page_head->pages);

    unsigned int level = page_head->levels;
    while (level > 0)
    {
        const page::EntryId entry_id =
            static_cast<page::EntryId>(page_id.Get() % kEntriesPerPage.Get());
        page_id = page_id / kEntriesPerPage.Get();
        const buffer::Pin<page::Offset> page{file_id,
                                             page::Id{PageHead::kLevelBegin[level] + page_id}};
        if (PageSet(page.GetPage(), entry_id, size))
        {
            size = PageGetRoot(page.GetPage());
        }
        else
        {
            break;
        }
        level--;
    }
}

static std::optional<page::Id> Find(catalog::FileId file_id, page::Offset value)
{
    const buffer::Pin<const PageHead> page_head{file_id, page::Id{0}};
    if (page_head->levels == 0)
    {
        return std::nullopt;
    }
    const buffer::Pin<const page::Offset> page{file_id, page::Id{PageHead::kLevelBegin[1]}};
    if (PageGetRoot(page.GetPage()) < value)
    {
        return std::nullopt;
    }
    page::Id page_id{PageGet(page.GetPage(), value).Get()};
    for (unsigned int level = 2; level <= page_head->levels; level++)
    {
        const buffer::Pin<const page::Offset> page{
            file_id, page::Id{PageHead::kLevelBegin[level] + page_id}};
        const page::EntryId entry_id = PageGet(page.GetPage(), value);
        page_id                      = page_id * static_cast<page::Id>(kEntriesPerPage.Get()) +
                  static_cast<page::Id>(entry_id.Get());
        ASSERT(page_id < page_head->pages);
    }
    return page_id;
}

std::pair<page::Id, bool> FindOrAppend(catalog::FileId file_id, page::Offset size)
{
    auto page_opt = Find(file_id, size);
    if (page_opt)
    {
        return std::make_pair(*page_opt, false);
    }
    return std::make_pair(Append(file_id, 0), true);
}

void Test()
{
    const unsigned int seed = os::Random();
    srand(seed);

    std::printf("testing fst\n");

    buffer::Init();
    catalog::Init();

    const catalog::FileId file_id = catalog::GetTableFileIds(catalog::TableId{}).fst;
    Init(file_id);

    std::vector<page::Offset>     test;
    static constexpr unsigned int kPageCount = 1'000;
    for (unsigned int i = 0; i < kPageCount; i++)
    {
        const page::Offset value_append = (rand() % 100'000U) + 1U;
        Append(file_id, value_append);
        test.push_back(value_append);
        static constexpr unsigned int kIterationCount = 10;
        for (unsigned int j = 0; j < kIterationCount; j++)
        {
            const page::Id     index_set(rand() % test.size());
            const page::Offset value_set = (rand() % 100'000U) + 1U;
            Update(file_id, index_set, value_set);
            test.at(index_set.Get())                 = value_set;
            const page::Offset            value_find = (rand() % 100'000U) + 1U;
            const std::optional<page::Id> page_id    = Find(file_id, value_find);
            const page::Offset            test_max   = *std::ranges::max_element(test);
            ASSERT((!page_id && test_max < value_find) ||
                   (page_id && test.at(page_id->Get()) >= value_find));
        }
    }

    buffer::Destroy();

    std::printf("testing fst done\n");
}
} // namespace fst
