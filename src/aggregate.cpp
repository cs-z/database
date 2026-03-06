#include "aggregate.hpp"
#include "common.hpp"
#include "iter.hpp"
#include "op.hpp"
#include "sort.hpp"
#include "value.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

void Aggregator::Init()
{
    min_   = {};
    max_   = {};
    sum_   = {};
    count_ = {};
}

void Aggregator::Feed(const ColumnValue& value)
{
    std::visit(
        Overload{
            [](const ColumnValueNull&)
            {
                // ignore null values
            },
            [](const ColumnValueBoolean&) { UNREACHABLE(); },
            [this](const ColumnValueInteger& value)
            {
                if (count_ == 0)
                {
                    min_ = value;
                    max_ = value;
                    sum_ = value;
                }
                else
                {
                    min_ = std::min(std::get<ColumnValueInteger>(min_), value);
                    max_ = std::max(std::get<ColumnValueInteger>(max_), value);
                    sum_ = std::get<ColumnValueInteger>(sum_) + value;
                }
                count_++;
            },
            [this](const ColumnValueReal& value)
            {
                if (count_ == 0)
                {
                    min_ = value;
                    max_ = value;
                    sum_ = value;
                }
                else
                {
                    min_ = std::min(std::get<ColumnValueReal>(min_), value);
                    max_ = std::max(std::get<ColumnValueReal>(max_), value);
                    sum_ = std::get<ColumnValueReal>(sum_) + value;
                }
                count_++;
            },
            [this](const ColumnValueVarchar& value)
            {
                if (count_ == 0)
                {
                    min_ = value;
                    max_ = value;
                }
                else
                {
                    if (CompareStrings(value, std::get<ColumnValueVarchar>(min_)) < 0)
                    {
                        min_ = value;
                    }
                    if (CompareStrings(value, std::get<ColumnValueVarchar>(max_)) > 0)
                    {
                        max_ = value;
                    }
                }
                count_++;
            },
        },
        value);
}

ColumnValue Aggregator::Get(Function function)
{
    switch (function)
    {
    case Function::kAvg:
        return std::visit(
            Overload{
                [](const ColumnValueNull& value) -> ColumnValue { return value; },
                [](const ColumnValueBoolean&) -> ColumnValue { UNREACHABLE(); },
                [this](const ColumnValueInteger& value) -> ColumnValue
                {
                    ASSERT(count_ > 0);
                    return value / count_;
                },
                [this](const ColumnValueReal& value) -> ColumnValue
                {
                    ASSERT(count_ > 0);
                    return value / static_cast<ColumnValueReal>(count_);
                },
                [](const ColumnValueVarchar&) -> ColumnValue { UNREACHABLE(); },
            },
            sum_);
    case Function::kMax:
        return max_;
    case Function::kMin:
        return min_;
    case Function::kSum:
        return sum_;
    case Function::kCount:
        return count_;
    }
    UNREACHABLE();
}

static Iter CreateIter(Iter&& parent, const Aggregates& aggregates)
{
    if (!aggregates.group_by.empty())
    {
        OrderBy order_by;
        for (const ColumnId column_id : aggregates.group_by)
        {
            order_by.columns.push_back({.column_id = column_id, .asc = true});
        }
        return std::make_unique<IterSort>(std::move(parent), std::move(order_by));
    }
    return parent;
}

IterAggregate::IterAggregate(Iter&& parent, Aggregates&& aggregates)
    : IterBase{parent->type}, parent_{CreateIter(std::move(parent), aggregates)},
      aggregates_{std::move(aggregates)}, aggregators_{aggregates_.exprs.size()}
{
}

void IterAggregate::Open()
{
    done_ = false;
    parent_->Open();
}

void IterAggregate::Restart()
{
    Open();
}

void IterAggregate::Close()
{
    parent_->Close();
}

std::optional<std::optional<Value>> IterAggregate::Feed(const Aggregates::GroupBy&  group_by,
                                                        const std::optional<Value>& value)
{
    // TODO

    // vars: old?, new?
    // return null: (!old && !new)
    // return value: (old && !new) || (old && new && old != new)
    // init if: !old || (old && new && old != new)
    // feed if: new

    std::optional<Value> value_key;
    if (value)
    {
        value_key = Value{};
        for (const ColumnId key_id : group_by)
        {
            value_key->push_back(value->at(key_id.Get()));
        }
    }

    const bool should_return_null = !current_key_ && !value_key;
    const bool key_changed = current_key_ && value_key && !ValueEqual(*current_key_, *value_key);
    const bool should_return_value = (current_key_ && !value_key) || key_changed;
    const bool should_init         = !current_key_ || key_changed;
    const bool should_feed         = value_key.has_value();

    std::optional<std::optional<Value>> result;

    if (should_return_null)
    {
        result = std::optional<Value>{std::nullopt};
    }

    if (should_return_value)
    {
        ASSERT(current_key_);
        Value result_value = std::move(*current_key_);
        for (std::size_t i = 0; i < aggregates_.exprs.size(); i++)
        {
            if (aggregates_.exprs[i].arg)
            {
                result_value.push_back(aggregators_[i].Get(aggregates_.exprs[i].function));
            }
            else
            {
                result_value.emplace_back(count_);
            }
        }
        result = {result_value};
    }

    if (should_init)
    {
        if (value_key)
        {
            current_key_ = std::move(value_key);
        }
        for (Aggregator& aggregator : aggregators_)
        {
            aggregator.Init();
        }
        count_ = 0;
    }

    if (should_feed)
    {
        ASSERT(value);
        for (std::size_t i = 0; i < aggregates_.exprs.size(); i++)
        {
            if (aggregates_.exprs[i].arg)
            {
                const ColumnValue column_value = aggregates_.exprs[i].arg->Eval(&*value);
                aggregators_[i].Feed(column_value);
            }
        }
        count_++;
    }

    return result;
}

std::optional<Value> IterAggregate::Next()
{
    if (done_)
    {
        return std::nullopt;
    }
    if (!aggregates_.group_by.empty())
    {
        for (;;)
        {
            const std::optional<Value> value = parent_->Next();
            if (!value)
            {
                done_ = true;
            }
            const std::optional<std::optional<Value>> result = Feed(aggregates_.group_by, value);
            if (result)
            {
                return *result;
            }
        }
    }
    else
    {
        for (Aggregator& aggregator : aggregators_)
        {
            aggregator.Init();
        }
        count_ = 0;
        for (;;)
        {
            const std::optional<Value> value = parent_->Next();
            if (!value)
            {
                break;
            }
            for (std::size_t i = 0; i < aggregates_.exprs.size(); i++)
            {
                if (aggregates_.exprs[i].arg)
                {
                    const ColumnValue column_value = aggregates_.exprs[i].arg->Eval(&*value);
                    aggregators_[i].Feed(column_value);
                }
            }
            count_++;
        }
        Value result;
        for (std::size_t i = 0; i < aggregates_.exprs.size(); i++)
        {
            if (aggregates_.exprs[i].arg)
            {
                result.push_back(aggregators_[i].Get(aggregates_.exprs[i].function));
            }
            else
            {
                result.emplace_back(count_);
            }
        }
        done_ = true;
        return result;
    }
}
