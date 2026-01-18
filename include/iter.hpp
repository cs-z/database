#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "value.hpp"
#include "expr.hpp"
#include "os.hpp"
#include "fst.hpp"

struct IterBase
{
	explicit IterBase(Type type) noexcept : type { std::move(type) } {}
	virtual ~IterBase() = default;

	virtual void open() = 0;
	virtual void restart() = 0;
	virtual void close() = 0;
	[[nodiscard]] virtual std::optional<Value> next() = 0;

	const Type type;
};

using Iter = std::unique_ptr<IterBase>;

class IterProject : public IterBase
{
public:

	IterProject(Iter &&parent, std::vector<ColumnId> &&columns)
		: IterBase { map_type(parent->type, columns) }
		, parent { std::move(parent) }
		, columns { std::move(columns) }
	{
	}
	~IterProject() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	static Type map_type(const Type &type, const std::vector<ColumnId> &columns);
	static Value map_value(Value &&value, const std::vector<ColumnId> &columns);

	Iter parent;
	const std::vector<ColumnId> columns;
};

class IterExpr : public IterBase
{
public:

	IterExpr(Iter &&parent, std::vector<ExprPtr> &&exprs, Type &&type)
		: IterBase { std::move(type) }
		, parent { std::move(parent) }
		, exprs { std::move(exprs) }
	{
	}
	~IterExpr() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	Iter parent;
	const std::vector<ExprPtr> exprs;
};

class IterScan : public IterBase
{
public:

	IterScan(catalog::FileIds file_ids, Type &&type)
		: IterBase { std::move(type) }
		, file_id { file_ids.dat }
		, page_count { fst::get_page_count(file_ids.fst) }
	{
	}
	~IterScan() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	const catalog::FileId file_id;
	const page::Id page_count;

	page::Id page_id;
	page::EntryId entry_id;

	buffer::Pin<const page::Slotted<>> page;
};

// TODO: remove
class IterScanTemp : public IterBase
{
public:

	IterScanTemp(os::TempFile &&file, page::Id page_count, Type &&type)
		: IterBase { std::move(type) }
		, file { std::move(file) }
		, page_count { page_count }
	{
	}
	~IterScanTemp() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	const os::TempFile file;
	const page::Id page_count;

	page::Id page_id;
	page::EntryId entry_id;

	buffer::Buffer<page::Slotted<>> page;
};

class IterJoinCross : public IterBase
{
public:

	IterJoinCross(Iter &&iter_l, Iter &&iter_r, Type &&type)
		: IterBase { std::move(type) }
		, iter_l { std::move(iter_l) }
		, iter_r { std::move(iter_r) }
	{
	}
	~IterJoinCross() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	Iter iter_l, iter_r;
	std::optional<Value> value_l;
};

class IterJoinQualified : public IterBase
{
public:

	IterJoinQualified(Iter &&iter_l, Iter &&iter_r, ExprPtr &&condition, Type &&type)
		: IterBase { type }
		, parent { std::make_unique<IterJoinCross>(std::move(iter_l), std::move(iter_r), std::move(type)) } // TODO
		, condition { std::move(condition) }
	{
	}
	~IterJoinQualified() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	Iter parent;
	const ExprPtr condition;
};

class IterFilter : public IterBase
{
public:

	IterFilter(Iter &&parent, ExprPtr &&condition)
		: IterBase { parent->type }
		, parent { std::move(parent) }
		, condition { std::move(condition) }
	{
	}
	~IterFilter() override = default;

	void open() override;
	void restart() override;
	void close() override;
	std::optional<Value> next() override;

private:

	Iter parent;
	const ExprPtr condition;
};
