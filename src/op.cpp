#include "op.hpp"
#include "error.hpp"

[[noreturn]] static void report_op1_type_error(SourceText op_text, std::optional<ColumnType> type)
{
	std::string type_name = type ? column_type_to_string(*type) : "NULL";
	throw ClientError { "operator not defined on type " + std::move(type_name), op_text };
}

[[noreturn]] static void report_op2_type_error(SourceText op_text, std::optional<ColumnType> type_l, std::optional<ColumnType> type_r)
{
	std::string type_l_name = type_l ? column_type_to_string(*type_l) : "NULL";
	std::string type_r_name = type_r ? column_type_to_string(*type_r) : "NULL";
	throw ClientError { "operator not defined on types " + std::move(type_l_name) + " and " + std::move(type_r_name), op_text };
}

std::string op1_to_string(Op1 op, const std::string &expr)
{
	switch (op) {
		case Op1::Pos: return "+" + ("(" + expr + ")");
		case Op1::Neg: return "-" + ("(" + expr + ")");
		case Op1::IsNull: return ("(" + expr + ")") + " IS NULL";
		case Op1::IsNotNull: return ("(" + expr + ")") + " IS NOT NULL";
		case Op1::Not: return "NOT " + ("(" + expr + ")");
	}
	UNREACHABLE();
}

int op1_prec(Op1 op)
{
	switch (op) {
		case Op1::Pos:       return 7;
		case Op1::Neg:       return 7;
		case Op1::IsNull:    return 3;
		case Op1::IsNotNull: return 3;
		case Op1::Not:       return 2;
	}
	UNREACHABLE();
}

int op2_prec(Op2 op)
{
	switch (op) {
		case Op2::ArithMul: return 6;
		case Op2::ArithDiv: return 6;
		case Op2::ArithMod: return 6;
		case Op2::ArithAdd: return 5;
		case Op2::ArithSub: return 5;
		case Op2::CompL:    return 4;
		case Op2::CompLe:   return 4;
		case Op2::CompG:    return 4;
		case Op2::CompGe:   return 4;
		case Op2::CompEq:   return 4;
		case Op2::CompNe:   return 4;
		case Op2::LogicAnd: return 1;
		case Op2::LogicOr:  return 0;
	}
	UNREACHABLE();
}

std::optional<ColumnType> op1_compile(const std::pair<Op1, SourceText> &op, std::optional<ColumnType> type)
{
	switch (op.first) {
		case Op1::Pos:
		case Op1::Neg:
			if (type && !column_type_is_arithmetic(*type)) {
				report_op1_type_error(op.second, type);
			}
			return type;
		case Op1::IsNull:
		case Op1::IsNotNull:
			return ColumnType::BOOLEAN;
		case Op1::Not:
			if (type != ColumnType::BOOLEAN) {
				report_op1_type_error(op.second, type);
			}
			return ColumnType::BOOLEAN;
	}
	UNREACHABLE();
}

ColumnValue op1_eval(Op1 op, const ColumnValue &value)
{
	switch (op) {
		case Op1::Pos: {
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					return ColumnValueNull {};
				},
				[](const ColumnValueBoolean &) -> ColumnValue {
					UNREACHABLE();
				},
				[](const ColumnValueInteger &value) -> ColumnValue {
					return +value;
				},
				[](const ColumnValueReal &value) -> ColumnValue {
					return +value;
				},
				[](const ColumnValueVarchar &) -> ColumnValue {
					UNREACHABLE();
				},
			}, value);
		}
		case Op1::Neg: {
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					return ColumnValueNull {};
				},
				[](const ColumnValueBoolean &) -> ColumnValue {
					UNREACHABLE();
				},
				[](const ColumnValueInteger &value) -> ColumnValue {
					return -value;
				},
				[](const ColumnValueReal &value) -> ColumnValue {
					return -value;
				},
				[](const ColumnValueVarchar &) -> ColumnValue {
					UNREACHABLE();
				},
			}, value);
		}
		case Op1::IsNull: {
			return value.index() == 0 ? Bool::TRUE : Bool::FALSE;
		}
		case Op1::IsNotNull: {
			return value.index() == 0 ? Bool::FALSE : Bool::TRUE;
		}
		case Op1::Not: {
			switch (std::get<ColumnValueBoolean>(value)) {
				case Bool::TRUE: return Bool::FALSE;
				case Bool::FALSE: return Bool::TRUE;
				case Bool::UNKNOWN: return Bool::UNKNOWN;
			}
		}
	}
	UNREACHABLE();
}

const char *op2_cstr(Op2 op)
{
	switch (op) {
		case Op2::ArithMul: return "*";
		case Op2::ArithDiv: return "/";
		case Op2::ArithMod: return "%";
		case Op2::ArithAdd: return "+";
		case Op2::ArithSub: return "-";
		case Op2::CompL: return "<";
		case Op2::CompLe: return "<=";
		case Op2::CompG: return ">";
		case Op2::CompGe: return ">=";
		case Op2::CompEq: return "=";
		case Op2::CompNe: return "<>";
		case Op2::LogicAnd: return "AND";
		case Op2::LogicOr: return "OR";
	}
	UNREACHABLE();
}

std::optional<ColumnType> op2_compile(const std::pair<Op2, SourceText> &op, std::optional<ColumnType> type_l, std::optional<ColumnType> type_r)
{
	switch (op.first) {
		case Op2::ArithMul:
		case Op2::ArithDiv:
		case Op2::ArithMod:
		case Op2::ArithSub: {
			if (type_l && !column_type_is_arithmetic(*type_l)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (type_r && !column_type_is_arithmetic(*type_r)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (!type_l || !type_r) {
				return std::nullopt;
			}
			ASSERT(*type_l == *type_r);
			return *type_l;
		}
		case Op2::ArithAdd: {
			if (type_l && !(column_type_is_arithmetic(*type_l) || *type_l == ColumnType::VARCHAR)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (type_r && !(column_type_is_arithmetic(*type_r) || *type_r == ColumnType::VARCHAR)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (!type_l || !type_r) {
				return std::nullopt;
			}
			ASSERT(*type_l == *type_r);
			return *type_l;
		}
		case Op2::CompL:
		case Op2::CompLe:
		case Op2::CompG:
		case Op2::CompGe: {
			if (type_l && !column_type_is_comparable(*type_l)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (type_r && !column_type_is_comparable(*type_r)) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			ASSERT(!type_l || !type_r || *type_l == *type_r);
			return ColumnType::BOOLEAN;
		}
		case Op2::CompEq:
		case Op2::CompNe: {
			ASSERT(!type_l || !type_r || *type_l == *type_r);
			return ColumnType::BOOLEAN;
		}
		case Op2::LogicAnd:
		case Op2::LogicOr: {
			if (type_l != ColumnType::BOOLEAN) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			if (type_r != ColumnType::BOOLEAN) {
				report_op2_type_error(op.second, type_l, type_r);
			}
			return ColumnType::BOOLEAN;
		}
	}
	UNREACHABLE();
}

ColumnValue op2_eval(const std::pair<Op2, SourceText> &op, const ColumnValue &value_l, const ColumnValue &value_r)
{
	ASSERT(value_l.index() == 0 || value_r.index() == 0 || value_l.index() == value_r.index());
	switch (op.first) {
		case Op2::ArithMul:
		case Op2::ArithDiv:
		case Op2::ArithMod:
		case Op2::ArithAdd:
		case Op2::ArithSub: {
			if (value_l.index() == 0 || value_r.index() == 0) {
				return ColumnValueNull {};
			}
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					UNREACHABLE();
				},
				[](const ColumnValueBoolean &) -> ColumnValue {
					UNREACHABLE();
				},
				[op, &value_r](const ColumnValueInteger &value) -> ColumnValue {
					const ColumnValueInteger a = value;
					const ColumnValueInteger b = std::get<ColumnValueInteger>(value_r);
					switch (op.first) {
						case Op2::ArithMul:
							return ColumnValueInteger { a * b };
						case Op2::ArithDiv:
							if (b == 0) {
								throw ClientError { "division by zero", op.second };
							}
							return ColumnValueInteger { a / b };
						case Op2::ArithMod:
							if (b == 0) {
								throw ClientError { "division by zero", op.second };
							}
							return ColumnValueInteger { a % b };
						case Op2::ArithAdd:
							return ColumnValueInteger { a + b };
						case Op2::ArithSub:
							return ColumnValueInteger { a - b };
						default:
							UNREACHABLE();
					}
				},
				[op, &value_r](const ColumnValueReal &value) -> ColumnValue {
					const ColumnValueReal a = value;
					const ColumnValueReal b = std::get<ColumnValueReal>(value_r);
					switch (op.first) {
						case Op2::ArithMul:
							return ColumnValueReal { a * b };
						case Op2::ArithDiv:
							if (b == 0) {
								throw ClientError { "division by zero", op.second };
							}
							return ColumnValueReal { a / b };
						case Op2::ArithAdd:
							return ColumnValueReal { a + b };
						case Op2::ArithSub:
							return ColumnValueReal { a - b };
						default:
							UNREACHABLE();
					}
				},
				[op, &value_r](const ColumnValueVarchar &value) -> ColumnValue {
					const ColumnValueVarchar &a = value;
					const ColumnValueVarchar &b = std::get<ColumnValueVarchar>(value_r);
					ASSERT(op.first == Op2::ArithAdd);
					return ColumnValueVarchar { a + b };
				},
			}, value_l);
		}
		case Op2::CompL:
		case Op2::CompLe:
		case Op2::CompG:
		case Op2::CompGe: {
			if (value_l.index() == 0 || value_r.index() == 0) {
				return Bool::UNKNOWN;
			}
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					UNREACHABLE();
				},
				[](const ColumnValueBoolean &) -> ColumnValue {
					UNREACHABLE();
				},
				[op, &value_r](const ColumnValueInteger &value) -> ColumnValue {
					const ColumnValueInteger a = value;
					const ColumnValueInteger b = std::get<ColumnValueInteger>(value_r);
					switch (op.first) {
						case Op2::CompL:
							return a < b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompLe:
							return a <= b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompG:
							return a > b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompGe:
							return a >= b ? Bool::TRUE : Bool::FALSE;
						default:
							UNREACHABLE();
					}
				},
				[op, &value_r](const ColumnValueReal &value) -> ColumnValue {
					const ColumnValueReal a = value;
					const ColumnValueReal b = std::get<ColumnValueReal>(value_r);
					switch (op.first) {
						case Op2::CompL:
							return a < b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompLe:
							return a <= b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompG:
							return a > b ? Bool::TRUE : Bool::FALSE;
						case Op2::CompGe:
							return a >= b ? Bool::TRUE : Bool::FALSE;
						default:
							UNREACHABLE();
					}
				},
				[op, &value_r](const ColumnValueVarchar &value) -> ColumnValue {
					const ColumnValueVarchar &a = value;
					const ColumnValueVarchar &b = std::get<ColumnValueVarchar>(value_r);
					switch (op.first) {
						case Op2::CompL:
							return compare_strings(a, b) < 0 ? Bool::TRUE : Bool::FALSE;
						case Op2::CompLe:
							return compare_strings(a, b) <= 0 ? Bool::TRUE : Bool::FALSE;
						case Op2::CompG:
							return compare_strings(a, b) > 0 ? Bool::TRUE : Bool::FALSE;
						case Op2::CompGe:
							return compare_strings(a, b) >= 0 ? Bool::TRUE : Bool::FALSE;
						default:
							UNREACHABLE();
					}
				},
			}, value_l);
		}
		case Op2::CompEq: {
			if (value_l.index() == 0 || value_r.index() == 0) {
				return Bool::UNKNOWN;
			}
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					UNREACHABLE();
				},
				[&value_r](const ColumnValueBoolean &value) -> ColumnValue {
					const ColumnValueBoolean a = value;
					const ColumnValueBoolean b = std::get<ColumnValueBoolean>(value_r);
					if (a == Bool::UNKNOWN) {
						return Bool::UNKNOWN;
					}
					if (b == Bool::UNKNOWN) {
						return Bool::UNKNOWN;
					}
					return a == b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueInteger &value) -> ColumnValue {
					const ColumnValueInteger a = value;
					const ColumnValueInteger b = std::get<ColumnValueInteger>(value_r);
					return a == b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueReal &value) -> ColumnValue {
					const ColumnValueReal a = value;
					const ColumnValueReal b = std::get<ColumnValueReal>(value_r);
					return a == b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueVarchar &value) -> ColumnValue {
					const ColumnValueVarchar &a = value;
					const ColumnValueVarchar &b = std::get<ColumnValueVarchar>(value_r);
					return compare_strings(a, b) == 0 ? Bool::TRUE : Bool::FALSE;
				},
			}, value_l);
		}
		case Op2::CompNe: {
			if (value_l.index() == 0 || value_r.index() == 0) {
				return Bool::UNKNOWN;
			}
			return std::visit(Overload{
				[](const ColumnValueNull &) -> ColumnValue {
					UNREACHABLE();
				},
				[&value_r](const ColumnValueBoolean &value) -> ColumnValue {
					const ColumnValueBoolean a = value;
					const ColumnValueBoolean b = std::get<ColumnValueBoolean>(value_r);
					if (a == Bool::UNKNOWN) {
						return Bool::UNKNOWN;
					}
					if (b == Bool::UNKNOWN) {
						return Bool::UNKNOWN;
					}
					return a != b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueInteger &value) -> ColumnValue {
					const ColumnValueInteger a = value;
					const ColumnValueInteger b = std::get<ColumnValueInteger>(value_r);
					return a != b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueReal &value) -> ColumnValue {
					const ColumnValueReal a = value;
					const ColumnValueReal b = std::get<ColumnValueReal>(value_r);
					return a != b ? Bool::TRUE : Bool::FALSE;
				},
				[&value_r](const ColumnValueVarchar &value) -> ColumnValue {
					const ColumnValueVarchar &a = value;
					const ColumnValueVarchar &b = std::get<ColumnValueVarchar>(value_r);
					return compare_strings(a, b) != 0 ? Bool::TRUE : Bool::FALSE;
				},
			}, value_l);
		}
		case Op2::LogicAnd: {
			const ColumnValueBoolean a = std::get<ColumnValueBoolean>(value_l);
			const ColumnValueBoolean b = std::get<ColumnValueBoolean>(value_r);
			switch (a) {
				case Bool::TRUE:
					switch (b) {
						case Bool::TRUE: return Bool::TRUE;
						case Bool::FALSE: return Bool::FALSE;
						case Bool::UNKNOWN: return Bool::UNKNOWN;
					}
					UNREACHABLE();
				case Bool::FALSE:
					switch (b) {
						case Bool::TRUE: return Bool::FALSE;
						case Bool::FALSE: return Bool::FALSE;
						case Bool::UNKNOWN: return Bool::FALSE;
					}
					UNREACHABLE();
				case Bool::UNKNOWN:
					switch (b) {
						case Bool::TRUE: return Bool::UNKNOWN;
						case Bool::FALSE: return Bool::UNKNOWN;
						case Bool::UNKNOWN: return Bool::UNKNOWN;
					}
					UNREACHABLE();
			}
			UNREACHABLE();
		}
		case Op2::LogicOr: {
			const ColumnValueBoolean a = std::get<ColumnValueBoolean>(value_l);
			const ColumnValueBoolean b = std::get<ColumnValueBoolean>(value_r);
			switch (a) {
				case Bool::TRUE:
					switch (b) {
						case Bool::TRUE: return Bool::TRUE;
						case Bool::FALSE: return Bool::TRUE;
						case Bool::UNKNOWN: return Bool::TRUE;
					}
					UNREACHABLE();
				case Bool::FALSE:
					switch (b) {
						case Bool::TRUE: return Bool::TRUE;
						case Bool::FALSE: return Bool::FALSE;
						case Bool::UNKNOWN: return Bool::UNKNOWN;
					}
					UNREACHABLE();
				case Bool::UNKNOWN:
					switch (b) {
						case Bool::TRUE: return Bool::TRUE;
						case Bool::FALSE: return Bool::UNKNOWN;
						case Bool::UNKNOWN: return Bool::UNKNOWN;
					}
					UNREACHABLE();
			}
			UNREACHABLE();
		}
	}
	UNREACHABLE();
}
