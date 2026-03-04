#pragma once

#include "catalog.hpp"
#include "page.hpp"

namespace fst
{
void                      Init(catalog::FileId file_id);
page::Id                  GetPageCount(catalog::FileId file_id);
std::pair<page::Id, bool> FindOrAppend(catalog::FileId file_id, page::Offset size);
void                      Update(catalog::FileId file_id, page::Id page_id, page::Offset size);
void                      Test();
} // namespace fst
