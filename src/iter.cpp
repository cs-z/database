#include "iter.hpp"
#include "buffer.hpp"
#include "common.hpp"
#include "expr.hpp"
#include "page.hpp"
#include "row.hpp"
#include "row_id.hpp"
#include "type.hpp"
#include "value.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

void IterProject::Open()
{
    parent_->Open();
}

void IterProject::Restart()
{
    parent_->Restart();
}

void IterProject::Close()
{
    parent_->Close();
}

std::optional<Value> IterProject::Next()
{
    std::optional<Value> value = parent_->Next();
    if (!value)
    {
        return std::nullopt;
    }
    return MapValue(std::move(*value), columns_);
}

Type IterProject::MapType(const Type& type, const std::vector<ColumnId>& columns)
{
    Type new_type;
    for (const ColumnId column : columns)
    {
        new_type.Push(type.At(column.Get()));
    }
    return new_type;
}

Value IterProject::MapValue(Value&& value, const std::vector<ColumnId>& columns)
{
    Value new_value;
    for (const ColumnId column : columns)
    {
        new_value.push_back(std::move(value.at(column.Get())));
    }
    return new_value;
}

void IterExpr::Open()
{
    parent_->Open();
}

void IterExpr::Restart()
{
    parent_->Restart();
}

void IterExpr::Close()
{
    parent_->Close();
}

std::optional<Value> IterExpr::Next()
{
    const std::optional<Value> value = parent_->Next();
    if (!value)
    {
        return std::nullopt;
    }
    Value result;
    for (const ExprPtr& expr : exprs_)
    {
        result.push_back(expr->Eval(&*value));
    }
    return result;
}

void IterScan::Open()
{
    page_id_  = {};
    entry_id_ = {};
}

void IterScan::Restart()
{
    Open();
}

void IterScan::Close()
{
    page_ = buffer::Pin<const page::Slotted<>>{};
}

std::optional<Value> IterScan::Next()
{
    for (;;)
    {
        if (entry_id_ == 0)
        {
            if (page_id_ == page_count_)
            {
                return std::nullopt;
            }
            page_ = buffer::Pin<const page::Slotted<>>{file_id_, page_id_};
        }
        if (entry_id_ == page_->GetEntryCount())
        {
            page_id_++;
            entry_id_ = page::EntryId{};
            continue;
        }
        const auto      curr_entry_id = entry_id_++;
        const U8* const entry         = page_->GetEntry(curr_entry_id);
        if (entry == nullptr)
        {
            continue;
        }
        auto value = row::Read(type, entry);
        if (emit_row_id_)
        {
            const auto row_id = PackRowId(page_id_, curr_entry_id);
            value.emplace_back(row_id); // TODO: is it safe? does not match type, hidden column
        }
        return value;
    }
}

void IterScanTemp::Open()
{
    page_id_  = {};
    entry_id_ = {};
}

void IterScanTemp::Restart()
{
    Open();
}

void IterScanTemp::Close()
{
}

std::optional<Value> IterScanTemp::Next()
{
    for (;;)
    {
        if (entry_id_ == 0)
        {
            if (page_id_ == page_count_)
            {
                return std::nullopt;
            }
            file_.Read(page_id_, page_.Get());
        }
        if (entry_id_ == page_->GetEntryCount())
        {
            page_id_++;
            entry_id_ = page::EntryId{};
            continue;
        }
        const U8* const entry = page_->GetEntry(entry_id_++);
        if (entry == nullptr)
        {
            continue;
        }
        return row::Read(type, entry);
    }
}

void IterJoinCross::Open()
{
    iter_l_->Open();
    iter_r_->Open();
    value_l_ = iter_l_->Next();
}

void IterJoinCross::Restart()
{
    iter_l_->Restart();
    iter_r_->Restart();
    value_l_ = iter_l_->Next();
}

void IterJoinCross::Close()
{
    iter_l_->Close();
    iter_r_->Close();
}

std::optional<Value> IterJoinCross::Next()
{
    for (;;)
    {
        if (!value_l_)
        {
            return std::nullopt;
        }
        std::optional<Value> value_r = iter_r_->Next();
        if (!value_r)
        {
            iter_r_->Restart();
            value_l_ = iter_l_->Next();
            continue;
        }
        Value value;
        value.insert(value.end(), value_l_->begin(), value_l_->end());
        value.insert(value.end(), value_r->begin(), value_r->end());
        return value;
    }
}

void IterJoinQualified::Open()
{
    parent_->Open();
}

void IterJoinQualified::Restart()
{
    parent_->Restart();
}

void IterJoinQualified::Close()
{
    parent_->Close();
}

std::optional<Value> IterJoinQualified::Next()
{
    for (;;)
    {
        std::optional<Value> value = parent_->Next();
        if (!value)
        {
            return std::nullopt;
        }
        const Bool condition_value = std::get<ColumnValueBoolean>(condition_->Eval(&*value));
        if (condition_value != Bool::kTrue)
        {
            continue;
        }
        return value;
    }
}

void IterFilter::Open()
{
    parent_->Open();
}

void IterFilter::Restart()
{
    parent_->Restart();
}

void IterFilter::Close()
{
    parent_->Close();
}

std::optional<Value> IterFilter::Next()
{
    for (;;)
    {
        std::optional<Value> value = parent_->Next();
        if (!value)
        {
            return std::nullopt;
        }
        const Bool condition_value = std::get<ColumnValueBoolean>(condition_->Eval(&*value));
        if (condition_value != Bool::kTrue)
        {
            continue;
        }
        return value;
    }
}
