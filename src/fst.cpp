#include <algorithm>

#include "fst.hpp"
#include "buffer.hpp"
#include "os.hpp"

// FREE SPACE TREE: stores free space of data pages, one entry per page

namespace fst
{
	constexpr page::EntryId ENTRIES_PER_PAGE { page::SIZE / sizeof(page::Offset) / 2 };

	struct PageHead
	{
		static constexpr unsigned int LEVEL_MAX = 6; // TODO

		static constexpr page::Id LEVEL_PAGES[LEVEL_MAX + 1] =
		{
			static_cast<page::Id>(0),
			static_cast<page::Id>(1),
			static_cast<page::Id>(ENTRIES_PER_PAGE.get()),
			static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()),
			static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()),
			static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()),
			static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) * static_cast<page::Id>(ENTRIES_PER_PAGE.get()),
		};

		static constexpr page::Id LEVEL_BEGIN[LEVEL_MAX + 1] =
		{
			page::Id { 1 },
			page::Id { 1 } + LEVEL_PAGES[0],
			page::Id { 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1],
			page::Id { 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2],
			page::Id { 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3],
			page::Id { 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3] + LEVEL_PAGES[4],
			page::Id { 1 } + LEVEL_PAGES[0] + LEVEL_PAGES[1] + LEVEL_PAGES[2] + LEVEL_PAGES[3] + LEVEL_PAGES[4] + LEVEL_PAGES[5],
		};

		unsigned int pages;
		unsigned int levels;
		unsigned int bottom; // number of pages in the bottom level

		inline void init()
		{
			pages = 0;
			levels = 0;
			bottom = 0;
		}

	};

	// static void page_print(const page::Offset *page)
	// {
	// 	ASSERT(page);
	// 	page::EntryId index { 1 };
	// 	page::EntryId count { 1 };
	// 	while (count <= ENTRIES_PER_PAGE) {
	// 		for (page::EntryId i {}; i < count; i++) {
	// 			printf("%u ", page[(index + i).get()]);
	// 		}
	// 		printf("\n");
	// 		index = index + count;
	// 		count = count * 2;
	// 	}
	// }

	static inline page::Offset page_get_root(const page::Offset *page)
	{
		ASSERT(page);
		return page[1];
	}

	static void page_init(page::Offset *page)
	{
		ASSERT(page);
		memset(page, 0, page::SIZE);
	}

	static bool page_set(page::Offset *page, page::EntryId entry_id, page::Offset size)
	{
		ASSERT(page);
		ASSERT(entry_id < ENTRIES_PER_PAGE);
		page::EntryId index = ENTRIES_PER_PAGE + entry_id;
		while (index > 0 && page[index.get()] != size) {
			page[index.get()] = size;
			index = index / 2;
			size = std::max(page[(index * 2).get()], page[(index * 2 + 1).get()]);
		}
		return index == 0;
	}

	static page::EntryId page_get(const page::Offset *page, page::Offset size)
	{
		ASSERT(page);
		ASSERT(size > 0);
		ASSERT(page_get_root(page) >= size);
		page::EntryId index { 1 };
		while (index < ENTRIES_PER_PAGE) {
			const page::EntryId index_l = index * 2;
			const page::EntryId index_r = index * 2 + 1;
			const page::Offset size_l = page[index_l.get()];
			const page::Offset size_r = page[index_r.get()];
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
		ASSERT(ENTRIES_PER_PAGE <= index && index < 2 * ENTRIES_PER_PAGE);
		ASSERT(page[index.get()] >= size);
		return index - ENTRIES_PER_PAGE;
	}

	void init(catalog::FileId file_id)
	{
		const buffer::Pin<PageHead> page_head { file_id, page::Id {}, true };
		page_head->init();
	}

	page::Id get_page_count(catalog::FileId file_id)
	{
		const buffer::Pin<const PageHead> page_head { file_id, page::Id {} };
		return page::Id { page_head->pages };
	}

	static page::Id append(catalog::FileId file_id, page::Offset value)
	{
		const buffer::Pin<PageHead> page_head { file_id, page::Id {} };
		if (page_head->pages % ENTRIES_PER_PAGE == 0) {
			if (page_head->bottom == PageHead::LEVEL_PAGES[page_head->levels]) {
				ASSERT(page_head->levels < PageHead::LEVEL_MAX);
				for (page::Id page_id {}; page_id < PageHead::LEVEL_PAGES[page_head->levels]; page_id++) {
					const buffer::Pin<const page::Offset> src { file_id, page::Id { PageHead::LEVEL_BEGIN[page_head->levels] + page_id } };
					const buffer::Pin<page::Offset> dst { file_id, page::Id { PageHead::LEVEL_BEGIN[page_head->levels + 1] + page_id }, true };
					memcpy(dst.get_page(), src.get_page(), page::SIZE);
				}
				for (unsigned int level = 1; level <= page_head->levels; level++) {
					for (page::Id page_id {}; page_id < PageHead::LEVEL_PAGES[level]; page_id++) {
						const buffer::Pin<page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[level] + page_id } };
						page_init(page.get_page());
					}
				}
				for (page::Id page_id {}; page_id < PageHead::LEVEL_PAGES[page_head->levels]; page_id++) {
					const buffer::Pin<const page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[page_head->levels + 1] + page_id } };
					const page::Offset value = page_get_root(page.get_page());
					update(file_id, page_id, value);
				}
				page_head->levels++;
			}
			const buffer::Pin<page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[page_head->levels] + page_head->bottom++ }, true };
			page_init(page.get_page());
		}
		const page::Id page_id = page::Id { page_head->pages++ };
		update(file_id, page_id, page::Offset { value });
		return page_id;
	}

	void update(catalog::FileId file_id, page::Id page_id, page::Offset size)
	{
		const buffer::Pin<const PageHead> page_head { file_id, page::Id { 0 } };
		ASSERT(page_id < page_head->pages);

		unsigned int level = page_head->levels;
		while (level > 0) {
			const page::EntryId entry_id = static_cast<page::EntryId>(page_id.get() %  ENTRIES_PER_PAGE.get());
			page_id = page_id / ENTRIES_PER_PAGE.get();
			const buffer::Pin<page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[level] + page_id } };
			if (page_set(page.get_page(), entry_id, size)) {
				size = page_get_root(page.get_page());
			}
			else {
				break;
			}
			level--;
		}
	}

	static std::optional<page::Id> find(catalog::FileId file_id, page::Offset value)
	{
		const buffer::Pin<const PageHead> page_head { file_id, page::Id { 0 } };
		if (page_head->levels == 0) {
			return std::nullopt;
		}
		const buffer::Pin<const page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[1] } };
		if (page_get_root(page.get_page()) < value) {
			return std::nullopt;
		}
		page::Id page_id { page_get(page.get_page(), page::Offset { value }).get() };
		for (unsigned int level = 2; level <= page_head->levels; level++) {
			const buffer::Pin<const page::Offset> page { file_id, page::Id { PageHead::LEVEL_BEGIN[level] + page_id } };
			const page::EntryId entry_id = page_get(page.get_page(), value);
			page_id = page_id * static_cast<page::Id>(ENTRIES_PER_PAGE.get()) + static_cast<page::Id>(entry_id.get());
			ASSERT(page_id < page_head->pages);
		}
		return page_id;
	}

	std::pair<page::Id, bool> find_or_append(catalog::FileId file_id, page::Offset size)
	{
		auto page_opt = find(file_id, size);
		if (page_opt) {
			return std::make_pair(*page_opt, false);
		}
		return std::make_pair(append(file_id, 0), true);
	}

	void test()
	{
		const unsigned int seed = os::random();
		srand(seed);

		printf("testing fst\n");

		buffer::init();
		catalog::init();

		const catalog::FileId file_id = catalog::get_table_file_ids(catalog::TableId {}).fst;
		init(file_id);

		std::vector<page::Offset> test;
		for (unsigned int i = 0; i < 1'000; i++) {
			const page::Offset value_append = rand() % 100'000U + 1U;
			append(file_id, value_append);
			test.push_back(value_append);
			for (unsigned int j = 0; j < 10; j++) {
				const page::Id index_set(rand() % test.size());
				const page::Offset value_set = rand() % 100'000U + 1U;
				update(file_id, index_set, value_set);
				test.at(index_set.get()) = value_set;
				const page::Offset value_find = rand() % 100'000U + 1U;
				const std::optional<page::Id> page_id = find(file_id, value_find);
				const page::Offset test_max = *std::max_element(test.cbegin(), test.cend());
				ASSERT((!page_id && test_max < value_find) || (page_id && test.at(page_id->get()) >= value_find));
			}
		}

		buffer::destroy();

		printf("testing fst done\n");
	}
}
