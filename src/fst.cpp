#include <algorithm>

#include "fst.hpp"
#include "buffer.hpp"
#include "os.hpp"

// FREE SPACE TREE: stores free space of data pages, one entry per page

namespace fst
{
	struct SlotTag {};
	using Slot = StrongId<SlotTag, u32>;

	static constexpr Slot SLOT_PER_PAGE { PAGE_SIZE.get() / sizeof(row::Size) / 2 };

	struct PageHead
	{
		static constexpr unsigned int LEVEL_MAX = 6; // TODO

		static constexpr PageId LEVEL_PAGES[LEVEL_MAX + 1] =
		{
			static_cast<PageId>(0),
			static_cast<PageId>(1),
			static_cast<PageId>(SLOT_PER_PAGE.get()),
			static_cast<PageId>(SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get()),
			static_cast<PageId>(SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get()),
			static_cast<PageId>(SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get()),
			static_cast<PageId>(SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get() * SLOT_PER_PAGE.get()),
		};

		static constexpr PageId LEVEL_BEGIN[LEVEL_MAX + 1] =
		{
			PageId{ 1 },
			PageId{ 1 } + LEVEL_PAGES[0],
			PageId{ 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1],
			PageId{ 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2],
			PageId{ 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3],
			PageId{ 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3] + LEVEL_PAGES[4],
			PageId{ 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3] + LEVEL_PAGES[4] + LEVEL_PAGES[5],
		};

		u32 pages;
		u32 levels;
		u32 bottom; // number of pages in the bottom level

		inline void init()
		{
			pages = 0;
			levels = 0;
			bottom = 0;
		}

	};

	static void page_print(const row::Size *page)
	{
		ASSERT(page);
		Slot index { 1 };
		Slot count { 1 };
		while (count <= SLOT_PER_PAGE) {
			for (Slot i {}; i < count; i++) {
				printf("%u ", page[(index + i).get()]);
			}
			printf("\n");
			index = index + count;
			count = count * 2;
		}
	}

	static inline row::Size page_get_root(const row::Size *page)
	{
		ASSERT(page);
		return page[1];
	}

	static void page_init(row::Size *page)
	{
		ASSERT(page);
		memset(page, 0, PAGE_SIZE.get());
	}

	static bool page_set(row::Size *page, Slot slot, row::Size size)
	{
		ASSERT(page);
		ASSERT(slot < SLOT_PER_PAGE);
		Slot index = SLOT_PER_PAGE + slot;
		while (index > 0 && page[index.get()] != size) {
			page[index.get()] = size;
			index = index / 2;
			size = std::max(page[(index * 2).get()], page[(index * 2 + 1).get()]);
		}
		return index == 0;
	}

	static Slot page_get(const row::Size *page, row::Size size)
	{
		ASSERT(page);
		ASSERT(size > 0);
		ASSERT(page_get_root(page) >= size);
		Slot index { 1 };
		while (index < SLOT_PER_PAGE) {
			const Slot index_l = index * 2;
			const Slot index_r = index * 2 + 1;
			const row::Size size_l = page[index_l.get()];
			const row::Size size_r = page[index_r.get()];
			// TODO: best fit or worst fit
			if (size_l >= size) {
				index = index_l;
				continue;
			}
			if (size_r >= size) {
				index = index_r;
				continue;
			}
			UNREACHABLE();
		}
		ASSERT(SLOT_PER_PAGE <= index && index < 2 * SLOT_PER_PAGE);
		ASSERT(page[index.get()] >= size);
		return index - SLOT_PER_PAGE;
	}

	void init(catalog::FileId file)
	{
		const buffer::Pin<PageHead> page_head { file, PageId {}, true };
		page_head->init();
	}

	PageId get_page_count(catalog::FileId file)
	{
		const buffer::Pin<const PageHead> page_head { file, PageId {} };
		return PageId { page_head->pages };
	}

	static PageId append(catalog::FileId file, u32 value)
	{
		const buffer::Pin<PageHead> page_head { file, PageId {} };
		if (page_head->pages % SLOT_PER_PAGE == 0) {
			if (page_head->bottom == PageHead::LEVEL_PAGES[page_head->levels]) {
				ASSERT(page_head->levels < PageHead::LEVEL_MAX);
				for (PageId page_id {}; page_id < PageHead::LEVEL_PAGES[page_head->levels]; page_id++) {
					const buffer::Pin<const row::Size> src { file, PageId { PageHead::LEVEL_BEGIN[page_head->levels] + page_id } };
					const buffer::Pin<row::Size> dst { file, PageId { PageHead::LEVEL_BEGIN[page_head->levels + 1] + page_id }, true };
					memcpy(dst.get_page(), src.get_page(), PAGE_SIZE.get());
				}
				for (u32 level = 1; level <= page_head->levels; level++) {
					for (PageId page_id {}; page_id < PageHead::LEVEL_PAGES[level]; page_id++) {
						const buffer::Pin<row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[level] + page_id } };
						page_init(page.get_page());
					}
				}
				for (PageId page_id {}; page_id < PageHead::LEVEL_PAGES[page_head->levels]; page_id++) {
					const buffer::Pin<const row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[page_head->levels + 1] + page_id } };
					const row::Size value = page_get_root(page.get_page());
					update(file, page_id, value);
				}
				page_head->levels++;
			}
			const buffer::Pin<row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[page_head->levels] + page_head->bottom++ }, true };
			page_init(page.get_page());
		}
		const PageId page_id = PageId { page_head->pages++ };
		update(file, page_id, row::Size { value });
		return page_id;
	}

	void update(catalog::FileId file, PageId page_id, row::Size size)
	{
		const buffer::Pin<const PageHead> page_head { file, PageId { 0 } };
		ASSERT(page_id < page_head->pages);

		u32 level = page_head->levels;
		while (level > 0) {
			const u32 entry_id = page_id.get() % SLOT_PER_PAGE.get();
			page_id = page_id / SLOT_PER_PAGE.get();
			const buffer::Pin<row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[level] + page_id } };
			if (page_set(page.get_page(), Slot { entry_id }, size)) {
				size = page_get_root(page.get_page());
			}
			else {
				break;
			}
			level--;
		}
	}

	static std::optional<PageId> find(catalog::FileId file, row::Size value)
	{
		const buffer::Pin<const PageHead> page_head { file, PageId { 0 } };
		if (page_head->levels == 0) {
			return std::nullopt;
		}
		const buffer::Pin<const row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[1] } };
		if (page_get_root(page.get_page()) < value) {
			return std::nullopt;
		}
		PageId page_id { page_get(page.get_page(), row::Size { value }).get() };
		for (u32 level = 2; level <= page_head->levels; level++) {
			const buffer::Pin<const row::Size> page { file, PageId { PageHead::LEVEL_BEGIN[level] + page_id } };
			const u32 entry_id = page_get(page.get_page(), row::Size { value }).get();
			page_id = page_id * SLOT_PER_PAGE.get() + entry_id;
			ASSERT(page_id < page_head->pages);
		}
		return page_id;
	}

	std::pair<PageId, bool> find_or_append(catalog::FileId file, row::Size size)
	{
		auto page_opt = find(file, size);
		if (page_opt) {
			return std::make_pair(*page_opt, false);
		}
		return std::make_pair(append(file, 0), true);
	}

	void test()
	{
		const unsigned int seed = os::random();
		srand(seed);

		printf("testing fst\n");

		buffer::init();
		catalog::init();

		const catalog::FileId file = catalog::get_table_files(catalog::TableId {}).first;
		fst::init(file);

		std::vector<row::Size> test;
		for (unsigned int i = 0; i < 1'000; i++) {
			const row::Size value_append = rand() % 100'000U + 1U;
			append(file, value_append);
			test.push_back(value_append);
			for (unsigned int j = 0; j < 10; j++) {
				const PageId index_set(rand() % test.size());
				const row::Size value_set = rand() % 100'000U + 1U;
				update(file, index_set, value_set);
				test.at(index_set.get()) = value_set;
				const row::Size value_find = rand() % 100'000U + 1U;
				const std::optional<PageId> page_id = find(file, value_find);
				const row::Size test_max = *std::max_element(test.cbegin(), test.cend());
				ASSERT((!page_id && test_max < value_find) || (page_id && test.at(page_id->get()) >= value_find));
			}
		}

		buffer::destroy();

		printf("testing fst done\n");
	}
}
