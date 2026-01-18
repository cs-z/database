#include "token.hpp"

const char* Token::tag_to_cstr(Tag tag)
{
    switch (tag)
    {
    case Tag::KwCreate:
        return "CREATE";
    case Tag::KwTable:
        return "TABLE";
    case Tag::KwDefault:
        return "DEFAULT";
    case Tag::KwConstraint:
        return "CONSTRAINT";
    case Tag::KwNot:
        return "NOT";
    case Tag::KwUnique:
        return "UNIQUE";
    case Tag::KwPrimary:
        return "PRIMARY";
    case Tag::KwKey:
        return "KEY";
    case Tag::KwCheck:
        return "CHECK";
    case Tag::KwDrop:
        return "DROP";
    case Tag::KwInsert:
        return "INSERT";
    case Tag::KwInto:
        return "INTO";
    case Tag::KwValues:
        return "VALUES";
    case Tag::KwSelect:
        return "SELECT";
    case Tag::KwDistinct:
        return "DISTINCT";
    case Tag::KwAll:
        return "ALL";
    case Tag::KwFrom:
        return "FROM";
    case Tag::KwAs:
        return "AS";
    case Tag::KwJoin:
        return "JOIN";
    case Tag::KwCross:
        return "CROSS";
    case Tag::KwInner:
        return "INNER";
    case Tag::KwOuter:
        return "OUTER";
    case Tag::KwLeft:
        return "LEFT";
    case Tag::KwRight:
        return "RIGHT";
    case Tag::KwFull:
        return "FULL";
    case Tag::KwOn:
        return "ON";
    case Tag::KwWhere:
        return "WHERE";
    case Tag::KwGroup:
        return "GROUP";
    case Tag::KwBy:
        return "BY";
    case Tag::KwHaving:
        return "HAVING";
    case Tag::KwUnion:
        return "UNION";
    case Tag::KwExcept:
        return "EXCEPT";
    case Tag::KwIntersect:
        return "INTERSECT";
    case Tag::KwOrder:
        return "ORDER";
    case Tag::KwAsc:
        return "ASC";
    case Tag::KwDesc:
        return "DESC";
    case Tag::KwInteger:
        return "INTEGER";
    case Tag::KwReal:
        return "REAL";
    case Tag::KwVarchar:
        return "VARCHAR";
    case Tag::KwBetween:
        return "BETWEEN";
    case Tag::KwIn:
        return "IN";
    case Tag::KwIs:
        return "IS";
    case Tag::KwExists:
        return "EXISTS";
    case Tag::KwCast:
        return "CAST";
    case Tag::KwNull:
        return "NULL";
    case Tag::KwLimit:
        return "LIMIT";
    case Tag::LParen:
        return "(";
    case Tag::RParen:
        return ")";
    case Tag::Period:
        return ".";
    case Tag::Comma:
        return ",";
    case Tag::Semicolon:
        return ";";
    case Tag::Asterisk:
        return "*";
    case Tag::Op2:
        return "an operator";
    case Tag::Function:
        return "an aggregation function";
    case Tag::Identifier:
        return "an identifier";
    case Tag::Constant:
        return "a constant";
    case Tag::End:
        return "end of source";
    }
    UNREACHABLE();
}