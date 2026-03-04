#pragma once

#include "common.hpp"
#include "iter.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct OrderBy
{
    struct Column
    {
        ColumnId column_id;
        bool     asc;
    };
    std::vector<Column> columns;
};

class IterSort : public IterBase
{
public:
    IterSort(Iter parent, OrderBy columns)
        : IterBase{parent->type}, parent_{std::move(parent)}, columns_{std::move(columns)}
    {
    }
    ~IterSort() override = default;

    void                 Open() override;
    void                 Restart() override;
    void                 Close() override;
    std::optional<Value> Next() override;

private:
    Iter          parent_;
    const OrderBy columns_;

    Iter sorted_iter_;
};
