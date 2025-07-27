#include "iter.hpp"
#include "fst.hpp"
#include "op.hpp"

IterProject::IterProject(IterPtr parent, std::vector<ColumnId> columns)
	: Iter { map_type(parent->type, columns) }
	, parent { std::move(parent) }
	, columns { std::move(columns) }
{
}

void IterProject::open()
{
	parent->open();
}

void IterProject::close()
{
	parent->close();
}

std::optional<Value> IterProject::next()
{
	std::optional<Value> value = parent->next();
	if (!value) {
		return std::nullopt;
	}
	return map_value(*value, columns);
}

Type IterProject::map_type(const Type &type, const std::vector<ColumnId> &columns)
{
	Type new_type;
	for (ColumnId column : columns) {
		new_type.push_back(type.at(column.get()));
	}
	return new_type;
}

Value IterProject::map_value(Value &value, const std::vector<ColumnId> &columns)
{
	Value new_value;
	for (ColumnId column : columns) {
		new_value.push_back(std::move(value.at(column.get())));
	}
	return new_value;
}

IterScan::IterScan(catalog::TableId table_id, Type type)
	: Iter { std::move(type) }
{
	const auto [file_fst, file_dat] = catalog::get_table_files(table_id);
	this->file = file_dat;
	this->page_count = fst::get_page_count(file_fst);
}

IterScan::IterScan(catalog::FileId file, PageId page_count, Type type)
	: Iter { std::move(type) }
	, file { file }
	, page_count { page_count }
{
}

void IterScan::open()
{
	page_id = PageId {};
	slot = row::Page::Slot {};
}

void IterScan::close()
{
	page = {};
}

std::optional<Value> IterScan::next()
{
	for (;;) {
		if (slot == 0) {
			if (page_id == page_count) {
				return std::nullopt;
			}
			page = { file, page_id };
		}
		if (slot == page->get_row_count()) {
			page_id++;
			slot = row::Page::Slot {};
			continue;
		}
		const u8 * const row = page->get_row(slot++);
		if (!row) {
			continue;
		}
		return row::read(type, row);
	}
}

IterScanTemp::IterScanTemp(os::Fd file, PageId page_count, Type type)
	: Iter { std::move(type) }
	, file { std::move(file) }
	, page_count { page_count }
{
}

void IterScanTemp::open()
{
	page_id = PageId {};
	slot = row::Page::Slot {};
}

void IterScanTemp::close()
{
}

std::optional<Value> IterScanTemp::next()
{
	for (;;) {
		if (slot == 0) {
			if (page_id == page_count) {
				return std::nullopt;
			}
			os::file_read(file, page_id, page.get());
		}
		if (slot == page->get_row_count()) {
			page_id++;
			slot = row::Page::Slot {};
			continue;
		}
		const u8 * const row = page->get_row(slot++);
		if (!row) {
			continue;
		}
		return row::read(type, row);
	}
}

IterExpr::IterExpr(IterPtr parent, std::vector<ExprPtr> exprs, Type type)
	: Iter { std::move(type) }
	, parent { std::move(parent) }
	, exprs { std::move(exprs) }
{
}

void IterExpr::open()
{
	parent->open();
}

void IterExpr::close()
{
	parent->close();
}

std::optional<Value> IterExpr::next()
{
	const std::optional<Value> value = parent->next();
	if (!value) {
		return std::nullopt;
	}
	Value result;
	for (const auto &expr : exprs) {
		result.push_back(expr->eval(&*value));
	}
	return result;
}

IterJoinCross::IterJoinCross(IterPtr iter_l, IterPtr iter_r, Type type)
	: Iter { std::move(type) }
	, iter_l { std::move(iter_l) }
	, iter_r { std::move(iter_r) }
{
}

void IterJoinCross::open()
{
	iter_l->open();
	iter_r->open();
	value_l = iter_l->next();
}

void IterJoinCross::close()
{
	iter_l->close();
	iter_r->close();
}

std::optional<Value> IterJoinCross::next()
{
	for (;;) {
		if (!value_l) {
			return std::nullopt;
		}
		std::optional<Value> value_r = iter_r->next();
		if (!value_r) {
			iter_r->close();
			iter_r->open();
			value_l = iter_l->next();
			continue;
		}
		Value value;
		value.insert(value.end(), value_l->begin(), value_l->end());
		value.insert(value.end(), value_r->begin(), value_r->end());
		return value;
	}
}

IterJoinQualified::IterJoinQualified(IterPtr iter_l, IterPtr iter_r, Type type, ExprPtr condition)
	: Iter { type }
	, parent { std::make_unique<IterJoinCross>(std::move(iter_l), std::move(iter_r), std::move(type)) } // TODO
	, condition { std::move(condition) }
{
}

void IterJoinQualified::open()
{
	parent->open();
}

void IterJoinQualified::close()
{
	parent->close();
}

std::optional<Value> IterJoinQualified::next()
{
	for (;;) {
		std::optional<Value> value = parent->next();
		if (!value) {
			return std::nullopt;
		}
		const Bool condition_value = std::get<ColumnValueBoolean>(condition->eval(&*value));
		if (condition_value != Bool::TRUE) {
			continue;
		}
		return value;
	}
}

IterFilter::IterFilter(IterPtr parent, ExprPtr condition)
	: Iter { parent->type }
	, parent { std::move(parent) }
	, condition { std::move(condition) }
{
}

void IterFilter::open()
{
	parent->open();
}

void IterFilter::close()
{
	parent->close();
}

std::optional<Value> IterFilter::next()
{
	for (;;) {
		std::optional<Value> value = parent->next();
		if (!value) {
			return std::nullopt;
		}
		const Bool condition_value = std::get<ColumnValueBoolean>(condition->eval(&*value));
		if (condition_value != Bool::TRUE) {
			continue;
		}
		return value;
	}
}
