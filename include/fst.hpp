#pragma once

#include "catalog.hpp"
#include "page.hpp"

namespace fst
{
	void init(catalog::FileId file);
	page::Id get_page_count(catalog::FileId file);
	std::pair<page::Id, bool> find_or_append(catalog::FileId file, page::Offset size);
	void update(catalog::FileId file, page::Id page_id, page::Offset size);
	void test();
}
