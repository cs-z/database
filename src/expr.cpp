#include "expr.hpp"

ColumnValue Expr::eval(const Value *value) const
{
	return std::visit(Overload{
		[](const Expr::DataConstant &expr) {
			return expr.value;
		},
		[&value](const Expr::DataColumn &expr) {
			ASSERT(value);
			return value->at(expr.column_id.get());
		},
		[&value](const Expr::DataCast &expr) {
			const ColumnValue column_value = expr.expr->eval(value);
			return column_value_eval_cast(column_value, expr.to);
		},
		[&value](const Expr::DataOp1 &expr) {
			const ColumnValue column_value = expr.expr->eval(value);
			return op1_eval(expr.op.first, column_value);
		},
		[&value](const Expr::DataOp2 &expr) {
			const ColumnValue column_value_l = expr.expr_l->eval(value);
			const ColumnValue column_value_r = expr.expr_r->eval(value);
			return op2_eval(expr.op, column_value_l, column_value_r);
		},
		[&value](const Expr::DataBetween &expr) {
			const ColumnValue column_value = expr.expr->eval(value);
			const ColumnValue column_value_min = expr.min->eval(value);
			const ColumnValue column_value_max = expr.max->eval(value);
			const ColumnValue comp_l = op2_eval({ expr.negated ? Op2::CompL : Op2::CompGe, expr.between_text }, column_value, column_value_min);
			const ColumnValue comp_r = op2_eval({ expr.negated ? Op2::CompG : Op2::CompLe, expr.between_text }, column_value, column_value_max);
			return op2_eval({ expr.negated ? Op2::LogicOr : Op2::LogicAnd, expr.between_text }, comp_l, comp_r);
		},
		[&value](const Expr::DataIn &expr) -> ColumnValue {
			const ColumnValue column_value = expr.expr->eval(value);
			if (column_value.index() == 0) {
				return Bool::UNKNOWN;
			}
			bool has_null = false;
			for (const ExprPtr &element_expr : expr.list) {
				const ColumnValue element = element_expr->eval(value);
				if (element.index() == 0) {
					has_null = true;
				}
				else if (element == column_value) {
					return expr.negated ? Bool::FALSE : Bool::TRUE;
				}
			}
			return has_null ? Bool::UNKNOWN : (expr.negated ? Bool::TRUE : Bool::FALSE);
		},
		[&value](const Expr::DataFunction &expr) {
			ASSERT(value);
			return value->at(expr.column_id.get());
		},
	}, data);
}
