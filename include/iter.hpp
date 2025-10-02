#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "value.hpp"
#include "expr.hpp"
#include "os.hpp"

struct Iter;
using IterPtr = std::unique_ptr<Iter>;

struct Iter
{
	Iter(Type type) : type { std::move(type) } {}
	virtual ~Iter() = default;
	virtual void open() = 0;
	virtual void close() = 0;
	virtual std::optional<Value> next() = 0;
	const Type type;
};

struct IterProject : Iter
{
	IterProject(IterPtr parent, std::vector<ColumnId> columns);
	~IterProject() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	Type map_type(const Type &type, const std::vector<ColumnId> &columns);
	Value map_value(Value &value, const std::vector<ColumnId> &columns);

	IterPtr parent;
	const std::vector<ColumnId> columns;
};

struct IterExpr : Iter
{
	IterExpr(IterPtr parent, std::vector<ExprPtr> exprs, Type type);
	~IterExpr() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	IterPtr parent;
	const std::vector<ExprPtr> exprs;
};

struct IterScan : Iter
{
	IterScan(catalog::TableId table_id, Type type);
	IterScan(catalog::FileId file, page::Id page_count, Type type);
	~IterScan() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	catalog::FileId file;
	page::Id page_count;

	page::Id page_id;
	page::SlotId slot_id;

	buffer::Pin<const page::PageSlotted> page;
};

struct IterScanTemp : Iter
{
	IterScanTemp(os::Fd file, page::Id page_count, Type type);
	~IterScanTemp() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	os::Fd file;
	page::Id page_count;

	page::Id page_id;
	page::SlotId slot_id;

	buffer::Buffer<page::PageSlotted> page;
};

struct IterJoinCross : Iter
{
	IterJoinCross(IterPtr iter_l, IterPtr iter_r, Type type);
	~IterJoinCross() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	IterPtr iter_l, iter_r;
	std::optional<Value> value_l;
};

struct IterJoinQualified : Iter
{
	IterJoinQualified(IterPtr iter_l, IterPtr iter_r, Type type, ExprPtr condition);
	~IterJoinQualified() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	IterPtr parent;
	const ExprPtr condition;
};

struct IterFilter : Iter
{
	IterFilter(IterPtr parent, ExprPtr condition);
	~IterFilter() override = default;

	void open() override;
	void close() override;
	std::optional<Value> next() override;

	IterPtr parent;
	const ExprPtr condition;
};
