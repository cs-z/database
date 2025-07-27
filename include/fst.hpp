#pragma once

#include "catalog.hpp"
#include "row.hpp"

namespace fst
{
	void init(catalog::FileId file);
	PageId get_page_count(catalog::FileId file);
	std::pair<PageId, bool> find_or_append(catalog::FileId file, row::Size size);
	void update(catalog::FileId file, PageId page_id, row::Size size);
	void test();
}
