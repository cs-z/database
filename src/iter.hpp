#pragma once

#include "buffer.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "expr.hpp"
#include "fst.hpp"
#include "os.hpp"
#include "page.hpp"
#include "type.hpp"
#include "value.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct IterBase
{
    explicit IterBase(Type type) noexcept : type{std::move(type)}
    {
    }
    virtual ~IterBase() = default;

    virtual void                               Open()    = 0;
    virtual void                               Restart() = 0;
    virtual void                               Close()   = 0;
    [[nodiscard]] virtual std::optional<Value> Next()    = 0;

    Type type;
};

using Iter = std::unique_ptr<IterBase>;

class IterProject : public IterBase
{
public:
    IterProject(Iter&& parent, std::vector<ColumnId>&& columns)
        : IterBase{MapType(parent->type, columns)}, parent_{std::move(parent)},
          columns_{std::move(columns)}
    {
    }
    ~IterProject() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    static Type  MapType(const Type& type, const std::vector<ColumnId>& columns);
    static Value MapValue(Value&& value, const std::vector<ColumnId>& columns);

    Iter                        parent_;
    const std::vector<ColumnId> columns_;
};

class IterExpr : public IterBase
{
public:
    IterExpr(Iter&& parent, std::vector<ExprPtr>&& exprs, Type&& type)
        : IterBase{std::move(type)}, parent_{std::move(parent)}, exprs_{std::move(exprs)}
    {
    }
    ~IterExpr() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    Iter                       parent_;
    const std::vector<ExprPtr> exprs_;
};

class IterScan : public IterBase
{
public:
    IterScan(catalog::FileIds file_ids, Type&& type, bool emit_row_id)
        : IterBase{std::move(type)}, emit_row_id_{emit_row_id}, file_id_{file_ids.dat},
          page_count_{fst::GetPageCount(file_ids.fst)}
    {
    }
    ~IterScan() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    const bool emit_row_id_;

    const catalog::FileId file_id_;
    const page::Id        page_count_;

    page::Id      page_id_;
    page::EntryId entry_id_;

    buffer::Pin<const page::Slotted<>> page_;
};

// TODO: remove
class IterScanTemp : public IterBase
{
public:
    IterScanTemp(os::TempFile&& file, page::Id page_count, Type&& type)
        : IterBase{std::move(type)}, file_{std::move(file)}, page_count_{page_count}
    {
    }
    ~IterScanTemp() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    const os::TempFile file_;
    const page::Id     page_count_;

    page::Id      page_id_;
    page::EntryId entry_id_;

    buffer::Buffer<page::Slotted<>> page_;
};

class IterJoinCross : public IterBase
{
public:
    IterJoinCross(Iter&& iter_l, Iter&& iter_r, Type&& type)
        : IterBase{std::move(type)}, iter_l_{std::move(iter_l)}, iter_r_{std::move(iter_r)}
    {
    }
    ~IterJoinCross() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    Iter                 iter_l_, iter_r_;
    std::optional<Value> value_l_;
};

class IterJoinQualified : public IterBase
{
public:
    IterJoinQualified(Iter&& iter_l, Iter&& iter_r, ExprPtr&& condition, Type&& type)
        : IterBase{type}, parent_{std::make_unique<IterJoinCross>(
                              std::move(iter_l), std::move(iter_r), std::move(type))} // TODO
          ,
          condition_{std::move(condition)}
    {
    }
    ~IterJoinQualified() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    Iter          parent_;
    const ExprPtr condition_;
};

class IterFilter : public IterBase
{
public:
    IterFilter(Iter&& parent, ExprPtr&& condition)
        : IterBase{parent->type}, parent_{std::move(parent)}, condition_{std::move(condition)}
    {
    }
    ~IterFilter() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    Iter          parent_;
    const ExprPtr condition_;
};
