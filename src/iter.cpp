#include "iter.hpp"
#include "op.hpp"
#include "row.hpp"

void IterProject::open()
{
	parent->open();
}

void IterProject::restart()
{
	parent->restart();
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
	return map_value(std::move(*value), columns);
}

Type IterProject::map_type(const Type &type, const std::vector<ColumnId> &columns)
{
	Type new_type;
	for (ColumnId column : columns) {
		new_type.push(type.at(column.get()));
	}
	return new_type;
}

Value IterProject::map_value(Value &&value, const std::vector<ColumnId> &columns)
{
	Value new_value;
	for (ColumnId column : columns) {
		new_value.push_back(std::move(value.at(column.get())));
	}
	return new_value;
}

void IterExpr::open()
{
	parent->open();
}

void IterExpr::restart()
{
	parent->restart();
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
	for (const ExprPtr &expr : exprs) {
		result.push_back(expr->eval(&*value));
	}
	return result;
}

void IterScan::open()
{
	page_id = {};
	entry_id = {};
}

void IterScan::restart()
{
	open();
}

void IterScan::close()
{
	page = {};
}

std::optional<Value> IterScan::next()
{
	for (;;) {
		if (entry_id == 0) {
			if (page_id == page_count) {
				return std::nullopt;
			}
			page = { file_id, page_id };
		}
		if (entry_id == page->get_entry_count()) {
			page_id++;
			entry_id = page::EntryId {};
			continue;
		}
		const u8 * const entry = page->get_entry(entry_id++);
		if (entry == nullptr) {
			continue;
		}
		return row::read(type, entry);
	}
}

void IterScanTemp::open()
{
	page_id = {};
	entry_id = {};
}

void IterScanTemp::restart()
{
	open();
}

void IterScanTemp::close()
{
}

std::optional<Value> IterScanTemp::next()
{
	for (;;) {
		if (entry_id == 0) {
			if (page_id == page_count) {
				return std::nullopt;
			}
			file.read(page_id, page.get());
		}
		if (entry_id == page->get_entry_count()) {
			page_id++;
			entry_id = page::EntryId {};
			continue;
		}
		const u8 * const entry = page->get_entry(entry_id++);
		if (entry == nullptr) {
			continue;
		}
		return row::read(type, entry);
	}
}

void IterJoinCross::open()
{
	iter_l->open();
	iter_r->open();
	value_l = iter_l->next();
}

void IterJoinCross::restart()
{
	iter_l->restart();
	iter_r->restart();
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
			iter_r->restart();
			value_l = iter_l->next();
			continue;
		}
		Value value;
		value.insert(value.end(), value_l->begin(), value_l->end());
		value.insert(value.end(), value_r->begin(), value_r->end());
		return value;
	}
}

void IterJoinQualified::open()
{
	parent->open();
}

void IterJoinQualified::restart()
{
	parent->restart();
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

void IterFilter::open()
{
	parent->open();
}

void IterFilter::restart()
{
	parent->restart();
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
