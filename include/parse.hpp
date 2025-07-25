#pragma once

#include "lexer.hpp"
#include "ast.hpp"

ColumnType parse_type(const std::string &name); // for catalog
AstQuery parse_query(Lexer &lexer);

AstInsertValue parse_insert_value(Lexer &lexer);
