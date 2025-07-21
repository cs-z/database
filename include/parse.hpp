#pragma once

#include "lexer.hpp"
#include "ast.hpp"

ColumnType parse_type(std::string name); // for catalog
AstQuery parse_query(Lexer &lexer);
