#include "expr.hpp"

ColumnValue Expr::eval(const Value &row) const
{
	return std::visit(Overload{
		[](const Expr::DataConstant &expr) {
			return expr.value;
		},
		[&row](const Expr::DataColumn &expr) {
			return row.at(expr.id);
		},
		[&row](const Expr::DataCast &expr) {
			const ColumnValue value = expr.expr->eval(row);
			return column_value_eval_cast(value, expr.to);
		},
		[&row](const Expr::DataOp1 &expr) {
			const ColumnValue value = expr.expr->eval(row);
			return op1_eval(expr.op.first, value);
		},
		[&row](const Expr::DataOp2 &expr) {
			const ColumnValue value_l = expr.expr_l->eval(row);
			const ColumnValue value_r = expr.expr_r->eval(row);
			return op2_eval(expr.op, value_l, value_r);
		},
		[&row](const Expr::DataBetween &expr) {
			const ColumnValue value = expr.expr->eval(row);
			const ColumnValue value_min = expr.min->eval(row);
			const ColumnValue value_max = expr.max->eval(row);
			const ColumnValue comp_l = op2_eval({ Op2::CompGe, expr.between_text }, value, value_min);
			const ColumnValue comp_r = op2_eval({ Op2::CompLe, expr.between_text }, value, value_max);
			return op2_eval({ Op2::LogicAnd, expr.between_text }, comp_l, comp_r);
		},
		[&row](const Expr::DataIn &expr) -> ColumnValue {
			const ColumnValue value = expr.expr->eval(row);
			if (value.index() == 0) {
				return Bool::UNKNOWN;
			}
			bool has_null = false;
			for (const std::unique_ptr<Expr> &element_expr : expr.list) {
				const ColumnValue element = element_expr->eval(row);
				if (element.index() == 0) {
					has_null = true;
				}
				else if (element == value) {
					return Bool::TRUE;
				}
			}
			return has_null ? Bool::UNKNOWN : Bool::FALSE;
		},
		[&row](const Expr::DataAggregate &expr) {
			return row.at(expr.id);
		},
	}, data);
}
