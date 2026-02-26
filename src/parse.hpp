#pragma once

#include "ast.hpp"
#include "lexer.hpp"

ColumnType   parse_type(const std::string& name);  // for catalog
AstStatement parse_statement(Lexer& lexer);
