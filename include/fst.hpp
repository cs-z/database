#pragma once

#include "catalog.hpp"
#include "page.hpp"

namespace fst
{
	void init(catalog::FileId file_id);
	page::Id get_page_count(catalog::FileId file_id);
	std::pair<page::Id, bool> find_or_append(catalog::FileId file_id, page::Offset size);
	void update(catalog::FileId file_id, page::Id page_id, page::Offset size);
	void test();
}
