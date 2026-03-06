#pragma once

#include "iter.hpp"
#include "op.hpp"
#include "value.hpp"

#include <optional>
#include <vector>

class Aggregator
{
public:
    void        Init();
    void        Feed(const ColumnValue& value);
    ColumnValue Get(Function function);

private:
    ColumnValue        min_;
    ColumnValue        max_;
    ColumnValue        sum_;
    ColumnValueInteger count_;
};

struct Aggregates
{
    struct Aggregate
    {
        Function function;
        ExprPtr  arg;
    };
    using GroupBy = std::vector<ColumnId>;
    std::vector<Aggregate> exprs;
    GroupBy                group_by;
};

class IterAggregate : public IterBase
{
public:
    IterAggregate(Iter&& parent, Aggregates&& aggregates);
    ~IterAggregate() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    std::optional<std::optional<Value>> Feed(const Aggregates::GroupBy&  group_by,
                                             const std::optional<Value>& value);

    Iter parent_;

    const Aggregates        aggregates_;
    std::optional<Value>    current_key_;
    std::vector<Aggregator> aggregators_;
    ColumnValueInteger      count_ = 0;
    bool                    done_  = false;
};
