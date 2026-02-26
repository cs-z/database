#pragma once

#include "common.hpp"
#include "error.hpp"
#include "op.hpp"
#include "type.hpp"
#include "value.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr
{
    struct DataConstant
    {
        ColumnValue value;
    };
    struct DataColumn
    {
        ColumnId column_id;
    };
    struct DataCast
    {
        ExprPtr    expr;
        ColumnType to;
    };
    struct DataOp1
    {
        ExprPtr                    expr;
        std::pair<Op1, SourceText> op;
    };
    struct DataOp2
    {
        ExprPtr                    expr_l, expr_r;
        std::pair<Op2, SourceText> op;
    };
    struct DataBetween
    {
        ExprPtr    expr, min, max;
        bool       negated;
        SourceText between_text;
    };
    struct DataIn
    {
        ExprPtr              expr;
        std::vector<ExprPtr> list;
        bool                 negated;
    };
    struct DataFunction
    {
        ColumnId column_id;
    };

    using Data = std::variant<DataConstant, DataColumn, DataCast, DataOp1, DataOp2, DataBetween,
                              DataIn, DataFunction>;

    Data                      data;
    std::optional<ColumnType> type;

    Expr(Data data, std::optional<ColumnType> type) : data{std::move(data)}, type{type}
    {
    }

    void        print() const;
    ColumnValue eval(const Value* value) const;
};
